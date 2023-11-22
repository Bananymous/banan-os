#pragma once

#include <kernel/Device/Device.h>
#include <kernel/Memory/VirtualRange.h>

namespace Kernel
{

	class FramebufferDevice : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<FramebufferDevice>> create_from_boot_framebuffer();
		~FramebufferDevice();

		virtual dev_t rdev() const override { return m_rdev; }

		virtual BAN::StringView name() const override { return m_name.sv(); }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

	private:
		FramebufferDevice(mode_t mode, uid_t uid, gid_t gid, dev_t rdev, paddr_t paddr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
		BAN::ErrorOr<void> initialize();

		void sync_pixels(uint32_t first_pixel, uint32_t pixel_count);

	private:
		const BAN::String m_name;
		const dev_t m_rdev;

		vaddr_t			m_video_memory_vaddr { 0 };
		const paddr_t	m_video_memory_paddr;
		const uint32_t	m_width;
		const uint32_t	m_height;
		const uint32_t	m_pitch;
		const uint8_t	m_bpp;

		BAN::UniqPtr<VirtualRange> m_video_buffer;
	};

}