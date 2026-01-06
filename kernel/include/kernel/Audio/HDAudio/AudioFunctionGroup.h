#pragma once

#include <kernel/Audio/Controller.h>
#include <kernel/Audio/HDAudio/Controller.h>

namespace Kernel
{

	class HDAudioController;

	class HDAudioFunctionGroup : public AudioController
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<HDAudioFunctionGroup>> create(BAN::RefPtr<HDAudioController>, uint8_t cid, HDAudio::AFGNode&&);

		void on_stream_interrupt(uint8_t stream_index);

	protected:
		// FIXME: allow setting these :D
		uint32_t get_channels() const override { return 2; }
		uint32_t get_sample_rate() const override { return 48000; }

		void handle_new_data() override;

	private:
		HDAudioFunctionGroup(BAN::RefPtr<HDAudioController> controller, uint8_t cid, HDAudio::AFGNode&& afg_node)
			: m_controller(controller)
			, m_afg_node(BAN::move(afg_node))
			, m_cid(cid)
		{ }
		~HDAudioFunctionGroup();

		BAN::ErrorOr<void> initialize();
		BAN::ErrorOr<void> initialize_stream();
		BAN::ErrorOr<void> initialize_output();
		BAN::ErrorOr<void> enable_output_path(uint8_t index);

		void reset_stream();

		BAN::ErrorOr<void> recurse_output_paths(const HDAudio::AFGWidget& widget, BAN::Vector<const HDAudio::AFGWidget*>& path);

		uint16_t get_format_data() const;
		uint16_t get_volume_data() const;

		size_t bdl_offset() const;

		void queue_bdl_data();

	private:
		static constexpr size_t m_max_path_length = 16;

		// use 6 512 sample BDL entries
		// each entry is ~10.7 ms at 48 kHz
		// -> total buffered audio is 64 ms
		static constexpr size_t m_bdl_entry_sample_frames = 512;
		static constexpr size_t m_bdl_entry_count         = 6;

		BAN::RefPtr<HDAudioController> m_controller;
		const HDAudio::AFGNode m_afg_node;
		const uint8_t m_cid;

		BAN::Vector<BAN::Vector<const HDAudio::AFGWidget*>> m_output_paths;
		size_t m_output_path_index { SIZE_MAX };

		uint8_t m_stream_id    { 0xFF };
		uint8_t m_stream_index { 0xFF };
		BAN::UniqPtr<DMARegion> m_bdl_region;

		size_t m_bdl_head { 0 };
		size_t m_bdl_tail { 0 };
		bool m_stream_running { false };
	};

}
