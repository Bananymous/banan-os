#include <kernel/Audio/HDAudio/AudioFunctionGroup.h>
#include <kernel/Audio/HDAudio/Registers.h>
#include <kernel/FS/DevFS/FileSystem.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<HDAudioFunctionGroup>> HDAudioFunctionGroup::create(BAN::RefPtr<HDAudioController> controller, uint8_t cid, HDAudio::AFGNode&& afg_node)
	{
		auto* audio_group_ptr = new HDAudioFunctionGroup(controller, cid, BAN::move(afg_node));
		if (audio_group_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto audio_group = BAN::RefPtr<HDAudioFunctionGroup>::adopt(audio_group_ptr);
		TRY(audio_group->initialize());
		return audio_group;
	}

	HDAudioFunctionGroup::~HDAudioFunctionGroup()
	{
		if (m_stream_id != 0xFF)
			m_controller->deallocate_stream_id(m_stream_id);
		if (m_stream_index != 0xFF)
			m_controller->deallocate_stream(m_stream_index);
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::initialize()
	{
		if constexpr(DEBUG_HDAUDIO)
		{
			const auto widget_to_string =
				[](HDAudio::AFGWidget::Type type) -> const char*
				{
					using HDAudio::AFGWidget;
					switch (type)
					{
						case AFGWidget::Type::OutputConverter: return "DAC";
						case AFGWidget::Type::InputConverter:  return "ADC";
						case AFGWidget::Type::Mixer:           return "Mixer";
						case AFGWidget::Type::Selector:        return "Selector";
						case AFGWidget::Type::PinComplex:      return "Pin";
						case AFGWidget::Type::Power:           return "Power";
						case AFGWidget::Type::VolumeKnob:      return "VolumeKnob";
						case AFGWidget::Type::BeepGenerator:   return "BeepGenerator";
					}
					ASSERT_NOT_REACHED();
				};

			dprintln("AFG {}", m_afg_node.id);
			for (auto widget : m_afg_node.widgets)
			{
				if (widget.type == HDAudio::AFGWidget::Type::PinComplex)
				{
					const uint32_t config = TRY(m_controller->send_command({
						.data = 0x00,
						.command = 0xF1C,
						.node_index = widget.id,
						.codec_address = m_cid,
					}));

					dprintln("  widget {}: {} ({}, {}), {32b}",
						widget.id,
						widget_to_string(widget.type),
						(int)widget.pin_complex.output,
						(int)widget.pin_complex.input,
						config
					);
				}
				else
				{
					dprintln("  widget {}: {}",
						widget.id,
						widget_to_string(widget.type)
					);
				}

				if (!widget.connections.empty())
					dprintln("    connections {}", widget.connections);
			}
		}

		TRY(initialize_stream());
		TRY(initialize_output());
		DevFileSystem::get().add_device(this);
		return {};
	}

	uint32_t HDAudioFunctionGroup::get_total_pins() const
	{
		uint32_t count = 0;
		for (const auto& widget : m_afg_node.widgets)
			if (widget.type == HDAudio::AFGWidget::Type::PinComplex && widget.pin_complex.output)
				count++;
		return count;
	}

	uint32_t HDAudioFunctionGroup::get_current_pin() const
	{
		const auto current_id = m_output_paths[m_output_path_index].front()->id;

		uint32_t pin = 0;
		for (const auto& widget : m_afg_node.widgets)
		{
			if (widget.type != HDAudio::AFGWidget::Type::PinComplex || !widget.pin_complex.output)
				continue;
			if (widget.id == current_id)
				return pin;
			pin++;
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::set_current_pin(uint32_t pin)
	{
		uint32_t pin_id = 0;
		for (const auto& widget : m_afg_node.widgets)
		{
			if (widget.type != HDAudio::AFGWidget::Type::PinComplex || !widget.pin_complex.output)
				continue;
			if (pin-- > 0)
				continue;
			pin_id = widget.id;
			break;
		}

		if (auto ret = disable_output_path(m_output_path_index); ret.is_error())
			dwarnln("failed to disable old output path {}", ret.error());

		for (size_t i = 0; i < m_output_paths.size(); i++)
		{
			if (m_output_paths[i].front()->id != pin_id)
				continue;

			if (auto ret = enable_output_path(i); !ret.is_error())
			{
				if (ret.error().get_error_code() == ENOTSUP)
					continue;
				dwarnln("path {} not supported", i);
				return ret.release_error();
			}

			dprintln("set output widget to {}", pin_id);
			m_output_path_index = i;
			return {};
		}

		dwarnln("failed to set output widget to {}", pin_id);

		return BAN::Error::from_errno(ENOTSUP);
	}

	size_t HDAudioFunctionGroup::bdl_offset() const
	{
		const size_t bdl_entry_bytes = m_bdl_entry_sample_frames * get_channels() * sizeof(uint16_t);
		const size_t bdl_total_size = bdl_entry_bytes * m_bdl_entry_count;
		if (auto rem = bdl_total_size % 128)
			return bdl_total_size + (128 - rem);
		return bdl_total_size;
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::initialize_stream()
	{
		const size_t bdl_entry_bytes = m_bdl_entry_sample_frames * get_channels() * sizeof(uint16_t);
		const size_t bdl_list_size   = m_bdl_entry_count * sizeof(HDAudio::BDLEntry);

		m_bdl_region = TRY(DMARegion::create(bdl_offset() + bdl_list_size));
		if (!m_controller->is_64bit() && (m_bdl_region->paddr() >> 32))
		{
			dwarnln("no 64 bit support but allocated bdl has 64 bit address :(");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto* bdl = reinterpret_cast<volatile HDAudio::BDLEntry*>(m_bdl_region->vaddr() + bdl_offset());
		for (size_t i = 0; i < m_bdl_entry_count; i++)
		{
			bdl[i].address = m_bdl_region->paddr() + i * bdl_entry_bytes;
			bdl[i].length = bdl_entry_bytes;
			bdl[i].ioc = 1;
		}

		ASSERT(m_stream_id == 0xFF);
		m_stream_id = TRY(m_controller->allocate_stream_id());

		ASSERT(m_stream_index == 0xFF);
		m_stream_index = TRY(m_controller->allocate_stream(HDAudio::StreamType::Output, this));

		reset_stream();

		return {};
	}

	void HDAudioFunctionGroup::reset_stream()
	{
		using Regs = HDAudio::Regs;

		auto& bar = m_controller->bar0();
		const auto base = 0x80 + m_stream_index * 0x20;

		const size_t bdl_entry_bytes = m_bdl_entry_sample_frames * get_channels() * sizeof(uint16_t);

		// stop stream
		bar.write8(base + Regs::SDCTL, bar.read8(base + Regs::SDCTL) & 0xFD);

		// reset stream
		bar.write8(base + Regs::SDCTL, (bar.read8(base + Regs::SDCTL) & 0xFE) | 1);
		while (!(bar.read8(base + Regs::SDCTL) & 1))
			Processor::pause();
		bar.write8(base + Regs::SDCTL, (bar.read8(base + Regs::SDCTL) & 0xFE));
		while ((bar.read8(base + Regs::SDCTL) & 1))
			Processor::pause();

		// set bdl address, total size and lvi
		const paddr_t bdl_paddr = m_bdl_region->paddr() + bdl_offset();
		bar.write32(base + Regs::SDBDPL, bdl_paddr);
		if (m_controller->is_64bit())
			bar.write32(base + Regs::SDBDPU, bdl_paddr >> 32);
		bar.write32(base + Regs::SDCBL, bdl_entry_bytes * m_bdl_entry_count);
		bar.write16(base + Regs::SDLVI, (bar.read16(base + Regs::SDLVI) & 0xFF00) | (m_bdl_entry_count - 1));

		// set stream format
		bar.write16(base + Regs::SDFMT, get_format_data());

		// set stream id, not bidirectional
		bar.write8(base + Regs::SDCTL + 2, (bar.read8(base + Regs::SDCTL + 2) & 0x07) | (m_stream_id << 4));

		m_bdl_head = 0;
		m_bdl_tail = 0;
		m_stream_running = false;
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::initialize_output()
	{
		BAN::Vector<const HDAudio::AFGWidget*> path;
		TRY(path.reserve(m_max_path_length));

		for (const auto& widget : m_afg_node.widgets)
		{
			if (widget.type != HDAudio::AFGWidget::Type::PinComplex || !widget.pin_complex.output)
				continue;
			TRY(path.push_back(&widget));
			TRY(recurse_output_paths(widget, path));
			path.pop_back();
		}

		dprintln_if(DEBUG_HDAUDIO, "found {} paths from output to DAC", m_output_paths.size());

		// select first supported path
		// FIXME: prefer associations
		// FIXME: does this pin even have a device?
		auto result = BAN::Error::from_errno(ENODEV);
		for (size_t i = 0; i < m_output_paths.size(); i++)
		{
			if (auto ret = enable_output_path(i); ret.is_error())
			{
				if (ret.error().get_error_code() != ENOTSUP)
					return ret.release_error();
				dwarnln("path {} not supported", i);
				result = BAN::Error::from_errno(ENOTSUP);
				continue;
			}

			m_output_path_index = i;
			break;
		}

		if (m_output_path_index >= m_output_paths.size())
		{
			dwarnln("could not find any usable output path");
			return result;
		}

		dprintln_if(DEBUG_HDAUDIO, "routed output path");
		for (const auto* widget : m_output_paths[m_output_path_index])
			dprintln_if(DEBUG_HDAUDIO, "  {}", widget->id);

		return {};
	}

	uint16_t HDAudioFunctionGroup::get_format_data() const
	{
		// TODO: don't hardcode this
		// format: PCM, 48 kHz, 16 bit, 2 channels
		return 0b0'0'000'000'0'001'0001;
	}

	uint16_t HDAudioFunctionGroup::get_volume_data() const
	{
		// TODO: don't hardcode this
		// left and right output, no mute, max gain
		return 0b1'0'1'1'0000'0'1111111;
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::enable_output_path(uint8_t index)
	{
		ASSERT(index < m_output_paths.size());
		const auto& path = m_output_paths[index];

		for (const auto* widget : path)
		{
			switch (widget->type)
			{
				using HDAudio::AFGWidget;
				case AFGWidget::Type::OutputConverter:
				case AFGWidget::Type::PinComplex:
					break;
				default:
					dwarnln("FIXME: support enabling widget type {}", static_cast<int>(widget->type));
					return BAN::Error::from_errno(ENOTSUP);
			}
		}

		const auto format = get_format_data();
		const auto volume = get_volume_data();

		for (size_t i = 0; i < path.size(); i++)
		{
			// set power state D0
			TRY(m_controller->send_command({
				.data = 0x00,
				.command = 0x705,
				.node_index = path[i]->id,
				.codec_address = m_cid,
			}));

			// set connection index
			if (i + 1 < path.size() && path[i]->connections.size() > 1)
			{
				uint8_t index = 0;
				for (; index < path[i]->connections.size(); index++)
					if (path[i]->connections[index] == path[i + 1]->id)
						break;
				ASSERT(index < path[i]->connections.size());

				TRY(m_controller->send_command({
					.data = index,
					.command = 0x701,
					.node_index = path[i]->id,
					.codec_address = m_cid,
				}));
			}

			// set volume
			TRY(m_controller->send_command({
				.data = static_cast<uint8_t>(volume & 0xFF),
				.command = static_cast<uint16_t>(0x300 | (volume >> 8)),
				.node_index = path[i]->id,
				.codec_address = m_cid,
			}));

			switch (path[i]->type)
			{
				using HDAudio::AFGWidget;

				case AFGWidget::Type::OutputConverter:
					// set stream and channel 0
					TRY(m_controller->send_command({
						.data = static_cast<uint8_t>(m_stream_id << 4),
						.command = 0x706,
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					// set format
					TRY(m_controller->send_command({
						.data = static_cast<uint8_t>(format & 0xFF),
						.command = static_cast<uint16_t>(0x200 | (format >> 8)),
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					break;

				case AFGWidget::Type::PinComplex:
					// enable output and H-Phn
					TRY(m_controller->send_command({
						.data = 0x80 | 0x40,
						.command = 0x707,
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					// enable EAPD
					TRY(m_controller->send_command({
						.data = 0x02,
						.command = 0x70C,
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					break;

				default:
					ASSERT_NOT_REACHED();
			}
		}

		return {};
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::disable_output_path(uint8_t index)
	{
		ASSERT(index < m_output_paths.size());
		const auto& path = m_output_paths[index];

		for (size_t i = 0; i < path.size(); i++)
		{
			// set power state D3
			TRY(m_controller->send_command({
				.data = 0x03,
				.command = 0x705,
				.node_index = path[i]->id,
				.codec_address = m_cid,
			}));

			switch (path[i]->type)
			{
				using HDAudio::AFGWidget;

				case AFGWidget::Type::OutputConverter:
					break;

				case AFGWidget::Type::PinComplex:
					// disable output and H-Phn
					TRY(m_controller->send_command({
						.data = 0x00,
						.command = 0x707,
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					// disable EAPD
					TRY(m_controller->send_command({
						.data = 0x00,
						.command = 0x70C,
						.node_index = path[i]->id,
						.codec_address = m_cid,
					}));
					break;

				default:
					ASSERT_NOT_REACHED();
			}
		}

		return {};
	}

	BAN::ErrorOr<void> HDAudioFunctionGroup::recurse_output_paths(const HDAudio::AFGWidget& widget, BAN::Vector<const HDAudio::AFGWidget*>& path)
	{
		// cycle "detection"
		if (path.size() >= m_max_path_length)
			return {};

		// we've reached a DAC
		if (widget.type == HDAudio::AFGWidget::Type::OutputConverter)
		{
			BAN::Vector<const HDAudio::AFGWidget*> path_copy;
			TRY(path_copy.resize(path.size()));
			for (size_t i = 0; i < path.size(); i++)
				path_copy[i] = path[i];
			TRY(m_output_paths.push_back(BAN::move(path_copy)));
			return {};
		}

		// check all connections
		for (const auto& connection : m_afg_node.widgets)
		{
			if (!widget.connections.contains(connection.id))
				continue;
			TRY(path.push_back(&connection));
			TRY(recurse_output_paths(connection, path));
			path.pop_back();
		}

		return {};
	}

	void HDAudioFunctionGroup::handle_new_data()
	{
		queue_bdl_data();
	}

	void HDAudioFunctionGroup::queue_bdl_data()
	{
		ASSERT(m_spinlock.current_processor_has_lock());

		const size_t bdl_entry_bytes = m_bdl_entry_sample_frames * get_channels() * sizeof(uint16_t);

		while ((m_bdl_head + 1) % m_bdl_entry_count != m_bdl_tail)
		{
			const size_t sample_data_tail = (m_sample_data_head + m_sample_data_capacity - m_sample_data_size) % m_sample_data_capacity;

			const size_t sample_frames = BAN::Math::min(m_sample_data_size / get_channels() / sizeof(uint16_t), m_bdl_entry_sample_frames);
			if (sample_frames == 0)
				break;

			const size_t copy_total_bytes = sample_frames * get_channels() * sizeof(uint16_t);
			const size_t copy_before_wrap = BAN::Math::min(copy_total_bytes, m_sample_data_capacity - sample_data_tail);

			memcpy(
				reinterpret_cast<void*>(m_bdl_region->vaddr() + m_bdl_head * bdl_entry_bytes),
				&m_sample_data[sample_data_tail],
				copy_before_wrap
			);

			if (copy_before_wrap < copy_total_bytes)
			{
				memcpy(
					reinterpret_cast<void*>(m_bdl_region->vaddr() + m_bdl_head * bdl_entry_bytes + copy_before_wrap),
					&m_sample_data[0],
					copy_total_bytes - copy_before_wrap
				);
			}

			if (copy_total_bytes < bdl_entry_bytes)
			{
				memset(
					reinterpret_cast<void*>(m_bdl_region->vaddr() + m_bdl_head * bdl_entry_bytes + copy_total_bytes),
					0x00,
					bdl_entry_bytes - copy_total_bytes
				);
			}

			m_sample_data_size -= copy_total_bytes;

			m_bdl_head = (m_bdl_head + 1) % m_bdl_entry_count;
		}

		if (m_bdl_head == m_bdl_tail || m_stream_running)
			return;

		// start the stream and enable IOC and descriptor error interrupts
		auto& bar = m_controller->bar0();
		const auto base = 0x80 + m_stream_index * 0x20;
		bar.write8(base + HDAudio::Regs::SDCTL, bar.read8(base + HDAudio::Regs::SDCTL) | 0x16);

		m_stream_running = true;
	}

	void HDAudioFunctionGroup::on_stream_interrupt(uint8_t stream_index)
	{
		using Regs = HDAudio::Regs;

		ASSERT(stream_index == m_stream_index);

		auto& bar = m_controller->bar0();
		const uint16_t base = 0x80 + stream_index * 0x20;

		const uint8_t sts = bar.read8(base + Regs::SDSTS);
		bar.write8(base + Regs::SDSTS, sts & 0x3C);

		if (sts & (1 << 4))
			derrorln("descriptor error");

		// ignore fifo errors as they are too common on real hw :D
		//if (sts & (1 << 3))
		//	derrorln("fifo error");

		if (sts & (1 << 2))
		{
			SpinLockGuard _(m_spinlock);

			ASSERT(m_stream_running);

			m_bdl_tail = (m_bdl_tail + 1) % m_bdl_entry_count;
			if (m_bdl_tail == m_bdl_head)
				reset_stream();

			queue_bdl_data();
		}
	}

}
