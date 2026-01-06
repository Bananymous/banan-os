#pragma once

#include <kernel/Audio/Controller.h>
#include <kernel/Audio/HDAudio/Definitions.h>
#include <kernel/Memory/DMARegion.h>

namespace Kernel
{

	class HDAudioController : public Interruptable, public BAN::RefCounted<HDAudioController>
	{
	public:
		static BAN::ErrorOr<void> create(PCI::Device& pci_device);

		BAN::ErrorOr<uint32_t> send_command(HDAudio::CORBEntry);

		uint8_t get_stream_index(HDAudio::StreamType type, uint8_t index) const;

		BAN::ErrorOr<uint8_t> allocate_stream_id();
		void deallocate_stream_id(uint8_t id);

		BAN::ErrorOr<uint8_t> allocate_stream(HDAudio::StreamType type, void* afg);
		void deallocate_stream(uint8_t index);

		PCI::BarRegion& bar0() { return *m_bar0; }

		bool is_64bit() const { return m_is64bit; }

		void handle_irq() override;

	private:
		HDAudioController(PCI::Device& pci_device)
			: m_pci_device(pci_device)
		{ }

		BAN::ErrorOr<void> initialize();
		BAN::ErrorOr<void> initialize_ring_buffers();

		BAN::ErrorOr<void> reset_controller();

		BAN::ErrorOr<HDAudio::Codec> initialize_codec(uint8_t codec);
		BAN::ErrorOr<HDAudio::AFGNode> initialize_node(uint8_t codec, uint8_t node);
		BAN::ErrorOr<HDAudio::AFGWidget> initialize_widget(uint8_t codec, uint8_t node);

	private:
		struct RingBuffer
		{
			vaddr_t vaddr;
			uint32_t index;
			uint32_t size;
		};

	private:
		PCI::Device& m_pci_device;
		BAN::UniqPtr<PCI::BarRegion> m_bar0;
		bool m_is64bit { false };

		bool m_use_immediate_command { false };

		uint8_t m_output_streams { 0 };
		uint8_t m_input_streams { 0 };
		uint8_t m_bidir_streams { 0 };
		void* m_allocated_streams[30] {};

		// NOTE: stream ids are from 1 to 15
		uint16_t m_allocated_stream_ids { 0 };

		Mutex m_command_mutex;
		SpinLock m_rb_lock;
		ThreadBlocker m_rb_blocker;

		RingBuffer m_corb;
		RingBuffer m_rirb;
		BAN::UniqPtr<DMARegion> m_ring_buffer_region;
	};

}
