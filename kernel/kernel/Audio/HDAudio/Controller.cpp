#include <kernel/Audio/HDAudio/AudioFunctionGroup.h>
#include <kernel/Audio/HDAudio/Controller.h>
#include <kernel/Audio/HDAudio/Registers.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/MMIO.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	BAN::ErrorOr<void> HDAudioController::create(PCI::Device& pci_device)
	{
		auto intel_hda_ptr = new HDAudioController(pci_device);
		if (intel_hda_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto intel_hda = BAN::RefPtr<HDAudioController>::adopt(intel_hda_ptr);
		TRY(intel_hda->initialize());
		return {};
	}

	BAN::ErrorOr<void> HDAudioController::initialize()
	{
		using Regs = HDAudio::Regs;

		m_pci_device.enable_bus_mastering();

		m_bar0 = TRY(m_pci_device.allocate_bar_region(0));

		dprintln("HD audio");
		dprintln("  revision {}.{}",
			m_bar0->read8(Regs::VMAJ),
			m_bar0->read8(Regs::VMIN)
		);

		const uint16_t global_cap = m_bar0->read16(Regs::GCAP);
		m_output_streams = (global_cap >> 12) & 0x0F;
		m_input_streams  = (global_cap >>  8) & 0x0F;
		m_bidir_streams  = (global_cap >>  3) & 0x1F;
		m_is64bit        = (global_cap & 1);

		if (m_output_streams + m_input_streams + m_bidir_streams > 30)
		{
			dwarnln("HD audio controller has {} streams, 30 is the maximum valid count",
				m_output_streams + m_input_streams + m_bidir_streams
			);
			return BAN::Error::from_errno(EINVAL);
		}

		dprintln("  output streams: {}", m_output_streams);
		dprintln("  input streams:  {}", m_input_streams);
		dprintln("  bidir streams:  {}", m_bidir_streams);
		dprintln("  64 bit support: {}", m_is64bit);

		TRY(reset_controller());

		if (auto ret = initialize_ring_buffers(); ret.is_error())
		{
			if (ret.error().get_error_code() != ETIMEDOUT)
				return ret.release_error();
			m_use_immediate_command = true;
		}

		TRY(m_pci_device.reserve_interrupts(1));
		m_pci_device.enable_interrupt(0, *this);
		m_bar0->write32(Regs::INTCTL, UINT32_MAX);

		for (uint8_t codec_id = 0; codec_id < 0x10; codec_id++)
		{
			auto codec_or_error = initialize_codec(codec_id);
			if (codec_or_error.is_error())
				continue;

			auto codec = codec_or_error.release_value();
			for (auto& node : codec.nodes)
				if (auto ret = HDAudioFunctionGroup::create(this, codec.id, BAN::move(node)); ret.is_error())
					dwarnln("Failed to initialize AFG: {}", ret.error());
		}

		return {};
	}

	BAN::ErrorOr<void> HDAudioController::reset_controller()
	{
		using HDAudio::Regs;

		const auto timeout_ms = SystemTimer::get().ms_since_boot() + 100;

		// transition into reset state
		if (const uint32_t gcap = m_bar0->read32(Regs::GCTL); gcap & 1)
		{
			m_bar0->write32(Regs::GCTL, gcap & 0xFFFFFEFC);
			while (m_bar0->read32(Regs::GCTL) & 1)
			{
				if (SystemTimer::get().ms_since_boot() > timeout_ms)
					return BAN::Error::from_errno(ETIMEDOUT);
				Processor::pause();
			}
		}

		m_bar0->write32(Regs::GCTL, (m_bar0->read32(Regs::GCTL) & 0xFFFFFEFC) | 1);
		while (!(m_bar0->read32(Regs::GCTL) & 1))
		{
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);
			Processor::pause();
		}

		// 4.3 The software must wait at least 521 us (25 frames) after reading CRST as a 1
		// before assuming that codecs have all made status change requests and have been
		// registered by the controller
		SystemTimer::get().sleep_ms(1);

		return {};
	}

	BAN::ErrorOr<void> HDAudioController::initialize_ring_buffers()
	{
		using Regs = HDAudio::Regs;

		// CORB: at most 1024 bytes (256 * uint32_t)
		// RIRB: at most 2048 bytes (256 * uint32_t * 2)
		m_ring_buffer_region = TRY(DMARegion::create(3 * 256 * sizeof(uint32_t)));

		struct SizeInfo
		{
			uint16_t size;
			uint8_t value;
		};

		const auto get_size_info =
			[](uint8_t byte) -> BAN::ErrorOr<SizeInfo>
			{
				if (byte & 0x40)
					return SizeInfo { 256, 2 };
				if (byte & 0x20)
					return SizeInfo {  16, 1 };
				if (byte & 0x10)
					return SizeInfo {   2, 0 };
				return BAN::Error::from_errno(EINVAL);
			};

		const auto corb_size = TRY(get_size_info(m_bar0->read8(Regs::CORBSIZE)));
		const auto rirb_size = TRY(get_size_info(m_bar0->read8(Regs::RIRBSIZE)));

		m_corb = {
			.vaddr = m_ring_buffer_region->vaddr(),
			.index = 1,
			.size = corb_size.size,
		};

		m_rirb = {
			.vaddr = m_ring_buffer_region->vaddr() + 1024,
			.index = 1,
			.size = rirb_size.size,
		};

		const paddr_t corb_paddr = m_ring_buffer_region->paddr();
		const paddr_t rirb_paddr = m_ring_buffer_region->paddr() + 1024;
		if (!m_is64bit && ((corb_paddr >> 32) || (rirb_paddr >> 32)))
		{
			dwarnln("no 64 bit support but allocated ring buffers have 64 bit addresses :(");
			return BAN::Error::from_errno(ENOTSUP);
		}

		// disable corb and rirb
		m_bar0->write8(Regs::CORBCTL,    (m_bar0->read8(Regs::CORBCTL) & 0xFC));
		m_bar0->write8(Regs::RIRBCTL,    (m_bar0->read8(Regs::RIRBCTL) & 0xF8));

		// set base address
		m_bar0->write32(Regs::CORBLBASE,  corb_paddr | (m_bar0->read32(Regs::CORBLBASE) & 0x0000007F));
		if (m_is64bit)
			m_bar0->write32(Regs::CORBUBASE,  corb_paddr >> 32);
		// set number of entries
		m_bar0->write8(Regs::CORBSIZE,   (m_bar0->read8(Regs::CORBSIZE) & 0xFC) | corb_size.value);
		// zero write pointer
		m_bar0->write16(Regs::CORBWP,    (m_bar0->read16(Regs::CORBWP) & 0xFF00));
		// reset read pointer
		const uint64_t corb_timeout_ms = SystemTimer::get().ms_since_boot() + 100;
		m_bar0->write16(Regs::CORBRP,    (m_bar0->read16(Regs::CORBRP) & 0x7FFF) | 0x8000);
		while (!(m_bar0->read16(Regs::CORBRP) & 0x8000))
		{
			if (SystemTimer::get().ms_since_boot() > corb_timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);
			Processor::pause();
		}
		m_bar0->write16(Regs::CORBRP,    (m_bar0->read16(Regs::CORBRP) & 0x7FFF));
		while ((m_bar0->read16(Regs::CORBRP) & 0x8000))
		{
			if (SystemTimer::get().ms_since_boot() > corb_timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);
			Processor::pause();
		}

		// set base address
		m_bar0->write32(Regs::RIRBLBASE,  rirb_paddr | (m_bar0->read32(Regs::RIRBLBASE) & 0x0000007F));
		if (m_is64bit)
			m_bar0->write32(Regs::RIRBUBASE,  rirb_paddr >> 32);
		// set number of entries
		m_bar0->write8(Regs::RIRBSIZE,   (m_bar0->read8(Regs::RIRBSIZE) & 0xFC) | rirb_size.value);
		// reset write pointer
		m_bar0->write16(Regs::RIRBWP,    (m_bar0->read16(Regs::RIRBWP)  & 0x7FFF) | 0x8000);
		// send interrupt on every packet
		m_bar0->write16(Regs::RINTCNT,   (m_bar0->read16(Regs::RINTCNT) & 0xFF00) | 0x01);

		// enable corb and rirb
		m_bar0->write8(Regs::CORBCTL,    (m_bar0->read8(Regs::CORBCTL) & 0xFC) | 3);
		m_bar0->write8(Regs::RIRBCTL,    (m_bar0->read8(Regs::RIRBCTL) & 0xF8) | 7);

		return {};
	}

	BAN::ErrorOr<HDAudio::Codec> HDAudioController::initialize_codec(uint8_t codec)
	{
		const auto resp = TRY(send_command({
			.data = 0x04,
			.command = 0xF00,
			.node_index = 0,
			.codec_address = codec,
		}));
		const uint8_t start = (resp >> 16) & 0xFF;
		const uint8_t count = (resp >>  0) & 0xFF;
		if (count == 0)
			return BAN::Error::from_errno(ENODEV);

		HDAudio::Codec result {};
		result.id = codec;
		TRY(result.nodes.reserve(count));

		for (size_t i = 0; i < count; i++)
			if (auto node_or_error = initialize_node(codec, start + i); !node_or_error.is_error())
				MUST(result.nodes.emplace_back(node_or_error.release_value()));

		return result;
	}

	BAN::ErrorOr<HDAudio::AFGNode> HDAudioController::initialize_node(uint8_t codec, uint8_t node)
	{
		{
			const auto resp = TRY(send_command({
				.data = 0x05,
				.command = 0xF00,
				.node_index = node,
				.codec_address = codec,
			}));
			const uint8_t type = (resp >> 0) & 0xFF;
			if (type != 0x01)
				return BAN::Error::from_errno(ENODEV);
		}

		const auto resp = TRY(send_command({
			.data = 0x04,
			.command = 0xF00,
			.node_index = node,
			.codec_address = codec,
		}));
		const uint8_t start = (resp >> 16) & 0xFF;
		const uint8_t count = (resp >>  0) & 0xFF;

		HDAudio::AFGNode result {};
		result.id = node;
		TRY(result.widgets.reserve(count));

		for (size_t i = 0; i < count; i++)
			if (auto widget_or_error = initialize_widget(codec, start + i); !widget_or_error.is_error())
				MUST(result.widgets.emplace_back(widget_or_error.release_value()));

		return result;
	}

	BAN::ErrorOr<HDAudio::AFGWidget> HDAudioController::initialize_widget(uint8_t codec, uint8_t widget)
	{
		const auto send_command_or_zero =
			[codec, widget, this](uint16_t cmd, uint8_t data) -> uint32_t
			{
				const auto command = HDAudio::CORBEntry {
					.data = data,
					.command = cmd,
					.node_index = widget,
					.codec_address = codec,
				};
				if (auto res = send_command(command); !res.is_error())
					return res.release_value();
				return 0;
			};

		using HDAudio::AFGWidget;
		const AFGWidget::Type type_list[] {
			AFGWidget::Type::OutputConverter,
			AFGWidget::Type::InputConverter,
			AFGWidget::Type::Mixer,
			AFGWidget::Type::Selector,
			AFGWidget::Type::PinComplex,
			AFGWidget::Type::Power,
			AFGWidget::Type::VolumeKnob,
			AFGWidget::Type::BeepGenerator,
		};

		const uint8_t type = (send_command_or_zero(0xF00, 0x09) >> 20) & 0x0F;
		if (type > sizeof(type_list) / sizeof(*type_list))
			return BAN::Error::from_errno(ENOTSUP);

		AFGWidget result {};
		result.type = type_list[type];
		result.id = widget;

		if (result.type == AFGWidget::Type::PinComplex)
		{
			const uint32_t cap = send_command_or_zero(0xF00, 0x0C);
			result.pin_complex.output = !!(cap & (1 << 4));
			result.pin_complex.input  = !!(cap & (1 << 5));
		}

		const uint8_t connection_info = send_command_or_zero(0xF00, 0x0E);
		const uint8_t conn_width = (connection_info & 0x80) ? 2 : 1;
		const uint8_t conn_count = connection_info & 0x3F;
		const uint16_t conn_mask = (1 << (8 * conn_width)) - 1;

		TRY(result.connections.resize(conn_count, 0));
		for (size_t i = 0; i < conn_count; i += 4 / conn_width)
		{
			const uint32_t conn = send_command_or_zero(0xF02, i);
			for (size_t j = 0; j < sizeof(conn) / conn_width && i + j < conn_count; j++)
				result.connections[i + j] = (conn >> (8 * conn_width * j)) & conn_mask;
		}

		return result;
	}

	BAN::ErrorOr<uint32_t> HDAudioController::send_command(HDAudio::CORBEntry command)
	{
		using Regs = HDAudio::Regs;

		// TODO: allow concurrent commands with CORB/RIRB
		LockGuard _(m_command_mutex);

		if (!m_use_immediate_command)
		{
			SpinLockGuard sguard(m_rb_lock);

			MMIO::write32(m_corb.vaddr + m_corb.index * sizeof(uint32_t), command.raw);
			m_bar0->write16(Regs::CORBWP, (m_bar0->read16(Regs::CORBWP) & 0xFF00) | m_corb.index);
			m_corb.index = (m_corb.index + 1) % m_corb.size;

			const uint64_t waketime_ms = SystemTimer::get().ms_since_boot() + 10;
			while ((m_bar0->read16(Regs::RIRBWP) & 0xFF) != m_rirb.index)
			{
				if (SystemTimer::get().ms_since_boot() > waketime_ms)
					return BAN::Error::from_errno(ETIMEDOUT);
				SpinLockGuardAsMutex smutex(sguard);
				m_rb_blocker.block_with_timeout_ms(10, &smutex);
			}

			const size_t offset = 2 * m_rirb.index * sizeof(uint32_t);
			m_rirb.index = (m_rirb.index + 1) % m_rirb.size;
			return MMIO::read32(m_rirb.vaddr + offset);
		}
		else
		{
			uint64_t waketime_ms = SystemTimer::get().ms_since_boot() + 10;
			while (m_bar0->read16(Regs::ICIS) & 1)
			{
				if (SystemTimer::get().ms_since_boot() > waketime_ms)
					break;
				Processor::pause();
			}

			// clear ICB if it did not clear "in reasonable timeout period"
			// and make sure IRV is cleared
			if (m_bar0->read16(Regs::ICIS) & 3)
				m_bar0->write16(Regs::ICIS, (m_bar0->read16(Regs::ICIS) & 0x00FC) | 2);

			m_bar0->write32(Regs::ICOI, command.raw);

			m_bar0->write16(Regs::ICIS, (m_bar0->read16(Regs::ICIS) & 0x00FC) | 1);

			waketime_ms = SystemTimer::get().ms_since_boot() + 10;
			while (!(m_bar0->read16(Regs::ICIS) & 2))
			{
				if (SystemTimer::get().ms_since_boot() > waketime_ms)
					return BAN::Error::from_errno(ETIMEDOUT);
				Processor::pause();
			}

			return m_bar0->read32(Regs::ICII);
		}
	}

	void HDAudioController::handle_irq()
	{
		using Regs = HDAudio::Regs;

		const uint32_t intsts = m_bar0->read32(Regs::INTSTS);
		if (!(intsts & (1u << 31)))
			return;

		if (intsts & (1 << 30))
		{
			if (const uint8_t rirbsts = m_bar0->read8(Regs::RIRBSTS) & ((1 << 2) | (1 << 0)))
			{
				if (rirbsts & (1 << 2))
					dwarnln("RIRB response overrun");
				if (rirbsts & (1 << 0))
				{
					SpinLockGuard _(m_rb_lock);
					m_rb_blocker.unblock();
				}
				m_bar0->write8(Regs::RIRBSTS, rirbsts);
			}

			if (const uint8_t corbsts = m_bar0->read8(Regs::CORBSTS) & (1 << 0))
			{
				dwarnln("CORB memory error");
				m_bar0->write8(Regs::CORBSTS, corbsts);
			}
		}

		for (size_t i = 0; i < 30; i++)
		{
			if (!(intsts & (1 << i)))
				continue;
			if (m_allocated_streams[i] == nullptr)
				dwarnln("interrupt from an unallocated stream??");
			else
				static_cast<HDAudioFunctionGroup*>(m_allocated_streams[i])->on_stream_interrupt(i);
		}
	}

	uint8_t HDAudioController::get_stream_index(HDAudio::StreamType type, uint8_t index) const
	{
		switch (type)
		{
			case HDAudio::StreamType::Bidirectional:
				index += m_output_streams;
				[[fallthrough]];
			case HDAudio::StreamType::Output:
				index += m_input_streams;
				[[fallthrough]];
			case HDAudio::StreamType::Input:
				break;
		}
		return index;
	}

	BAN::ErrorOr<uint8_t> HDAudioController::allocate_stream_id()
	{
		for (uint8_t id = 1; id < 16; id++)
		{
			if (m_allocated_stream_ids & (1 << id))
				continue;
			m_allocated_stream_ids |= 1 << id;
			return id;
		}

		return BAN::Error::from_errno(EAGAIN);
	}

	void HDAudioController::deallocate_stream_id(uint8_t id)
	{
		ASSERT(m_allocated_stream_ids & (1 << id));
		m_allocated_stream_ids &= ~(1 << id);
	}

	BAN::ErrorOr<uint8_t> HDAudioController::allocate_stream(HDAudio::StreamType type, void* afg)
	{
		const uint8_t stream_count_lookup[] {
			[(int)HDAudio::StreamType::Input]         = m_input_streams,
			[(int)HDAudio::StreamType::Output]        = m_output_streams,
			[(int)HDAudio::StreamType::Bidirectional] = m_bidir_streams,
		};

		const uint8_t stream_count = stream_count_lookup[static_cast<int>(type)];
		for (uint8_t i = 0; i < stream_count; i++)
		{
			const uint8_t index = get_stream_index(type, i);
			if (m_allocated_streams[index])
				continue;
			m_allocated_streams[index] = afg;
			return index;
		}

		return BAN::Error::from_errno(EAGAIN);
	}

	void HDAudioController::deallocate_stream(uint8_t index)
	{
		ASSERT(m_allocated_streams[index]);
		m_allocated_streams[index] = nullptr;
		// TODO: maybe make sure the stream is stopped/reset (?)
	}

}
