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

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

		uint32_t get_pixel(uint32_t x, uint32_t y) const;
		void set_pixel(uint32_t x, uint32_t y, uint32_t rgb);

		// positive rows -> empty pixels on bottom
		// negative rows -> empty pixels on top
		void scroll(int32_t rows, uint32_t rgb);

		void sync_pixels_full();
		void sync_pixels_linear(uint32_t first_pixel, uint32_t pixel_count);
		void sync_pixels_rectangle(uint32_t top_right_x, uint32_t top_right_y, uint32_t width, uint32_t height);

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> mmap_region(PageTable&, off_t offset, size_t len, AddressRange, MemoryRegion::Type, PageTable::flags_t) override;

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const override { return m_name.sv(); }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override { return false; }

	private:
		FramebufferDevice(mode_t mode, uid_t uid, gid_t gid, dev_t rdev, paddr_t paddr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
		BAN::ErrorOr<void> initialize();

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

		friend class FramebufferMemoryRegion;
	};

}
