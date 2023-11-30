#include <kernel/BootInfo.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Memory/Heap.h>

#include <sys/framebuffer.h>
#include <sys/mman.h>

namespace Kernel
{

	static uint32_t get_framebuffer_device_index()
	{
		static uint32_t index = 0;
		return index++;
	}

	BAN::ErrorOr<BAN::RefPtr<FramebufferDevice>> FramebufferDevice::create_from_boot_framebuffer()
	{
		if (g_boot_info.framebuffer.type != FramebufferType::RGB)
			return BAN::Error::from_errno(ENODEV);
		if (g_boot_info.framebuffer.bpp != 24 && g_boot_info.framebuffer.bpp != 32)
			return BAN::Error::from_errno(ENOTSUP);
		auto* device_ptr = new FramebufferDevice(
			0666, 0, 0,
			DevFileSystem::get().get_next_dev(),
			g_boot_info.framebuffer.address,
			g_boot_info.framebuffer.width,
			g_boot_info.framebuffer.height,
			g_boot_info.framebuffer.pitch,
			g_boot_info.framebuffer.bpp
		);
		if (device_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto device = BAN::RefPtr<FramebufferDevice>::adopt(device_ptr);
		TRY(device->initialize());
		return device;
	}

	FramebufferDevice::FramebufferDevice(mode_t mode, uid_t uid, gid_t gid, dev_t rdev, paddr_t paddr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp)
		: CharacterDevice(mode, uid, gid)
		, m_name(BAN::String::formatted("fb{}", get_framebuffer_device_index()))
		, m_rdev(rdev)
		, m_video_memory_paddr(paddr)
		, m_width(width)
		, m_height(height)
		, m_pitch(pitch)
		, m_bpp(bpp)
	{ }

	FramebufferDevice::~FramebufferDevice()
	{
		if (m_video_memory_vaddr == 0)
			return;
		size_t video_memory_pages = range_page_count(m_video_memory_paddr, m_height * m_pitch);
		PageTable::kernel().unmap_range(m_video_memory_vaddr, video_memory_pages * PAGE_SIZE);
	}

	BAN::ErrorOr<void> FramebufferDevice::initialize()
	{
		size_t video_memory_pages = range_page_count(m_video_memory_paddr, m_height * m_pitch);
		m_video_memory_vaddr = PageTable::kernel().reserve_free_contiguous_pages(video_memory_pages, KERNEL_OFFSET);
		if (m_video_memory_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		PageTable::kernel().map_range_at(
			m_video_memory_paddr & PAGE_ADDR_MASK,
			m_video_memory_vaddr,
			video_memory_pages * PAGE_SIZE,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present
		);

		m_video_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET, UINTPTR_MAX,
			BAN::Math::div_round_up<size_t>(m_width * m_height * (BANAN_FB_BPP / 8), PAGE_SIZE) * PAGE_SIZE,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		return {};
	}

	BAN::ErrorOr<size_t> FramebufferDevice::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		// Reading from negative offset will fill buffer with framebuffer info
		if (offset < 0)
		{
			if (buffer.size() < sizeof(framebuffer_info_t))
				return BAN::Error::from_errno(ENOBUFS);

			auto& fb_info = buffer.as<framebuffer_info_t>();
			fb_info.width = m_width;
			fb_info.height = m_height;

			return sizeof(framebuffer_info_t);
		}

		if ((size_t)offset >= m_width * m_height * (BANAN_FB_BPP / 8))
			return 0;
		
		size_t bytes_to_copy = BAN::Math::min<size_t>(m_width * m_height * (BANAN_FB_BPP / 8) - offset, buffer.size());
		memcpy(buffer.data(), reinterpret_cast<void*>(m_video_buffer->vaddr() + offset), bytes_to_copy);

		return bytes_to_copy;
	}

	BAN::ErrorOr<size_t> FramebufferDevice::write_impl(off_t offset, BAN::ConstByteSpan buffer)
	{
		if (offset < 0)
			return BAN::Error::from_errno(EINVAL);
		if ((size_t)offset >= m_width * m_height * (BANAN_FB_BPP / 8))
			return 0;
		
		size_t bytes_to_copy = BAN::Math::min<size_t>(m_width * m_height * (BANAN_FB_BPP / 8) - offset, buffer.size());
		memcpy(reinterpret_cast<void*>(m_video_buffer->vaddr() + offset), buffer.data(), bytes_to_copy);

		uint32_t first_pixel = offset / (BANAN_FB_BPP / 8);
		uint32_t pixel_count = BAN::Math::div_round_up<uint32_t>(bytes_to_copy + (offset % (BANAN_FB_BPP / 8)), (BANAN_FB_BPP / 8));
		sync_pixels_linear(first_pixel, pixel_count);

		return bytes_to_copy;
	}

	void FramebufferDevice::set_pixel(uint32_t x, uint32_t y, uint32_t rgb)
	{
		if (x >= m_width || y >= m_height)
			return;
		auto* video_buffer_u8 = reinterpret_cast<uint8_t*>(m_video_buffer->vaddr());
		video_buffer_u8[(y * m_width + x) * (BANAN_FB_BPP / 8) + 0] = rgb >>  0;
		video_buffer_u8[(y * m_width + x) * (BANAN_FB_BPP / 8) + 1] = rgb >>  8;
		video_buffer_u8[(y * m_width + x) * (BANAN_FB_BPP / 8) + 2] = rgb >> 16;
		if constexpr(BANAN_FB_BPP == 32)
			video_buffer_u8[(y * m_width + x) * (BANAN_FB_BPP / 8) + 3] = rgb >> 24;
	}

	void FramebufferDevice::sync_pixels_full()
	{
		auto* video_memory_u8 = reinterpret_cast<uint8_t*>(m_video_memory_vaddr);
		auto* video_buffer_u8 = reinterpret_cast<uint8_t*>(m_video_buffer->vaddr());

		for (uint32_t i = 0; i < m_width * m_height; i++)
		{
			uint32_t row = i / m_width;
			uint32_t idx = i % m_width;

			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 0] = video_buffer_u8[i * (BANAN_FB_BPP / 8) + 0];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 1] = video_buffer_u8[i * (BANAN_FB_BPP / 8) + 1];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 2] = video_buffer_u8[i * (BANAN_FB_BPP / 8) + 2];
		}
	}

	void FramebufferDevice::sync_pixels_linear(uint32_t first_pixel, uint32_t pixel_count)
	{
		if (first_pixel >= m_width * m_height)
			return;
		if (first_pixel + pixel_count > m_width * m_height)
			pixel_count = m_width * m_height - first_pixel;

		auto* video_memory_u8 = reinterpret_cast<uint8_t*>(m_video_memory_vaddr);
		auto* video_buffer_u8 = reinterpret_cast<uint8_t*>(m_video_buffer->vaddr());

		for (uint32_t i = 0; i < pixel_count; i++)
		{
			uint32_t row = (first_pixel + i) / m_width;
			uint32_t idx = (first_pixel + i) % m_width;

			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 0] = video_buffer_u8[(first_pixel + i) * (BANAN_FB_BPP / 8) + 0];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 1] = video_buffer_u8[(first_pixel + i) * (BANAN_FB_BPP / 8) + 1];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 2] = video_buffer_u8[(first_pixel + i) * (BANAN_FB_BPP / 8) + 2];
		}
	}

	void FramebufferDevice::sync_pixels_rectangle(uint32_t top_right_x, uint32_t top_right_y, uint32_t width, uint32_t height)
	{
		if (top_right_x >= m_width || top_right_y >= m_height)
			return;
		if (top_right_x + width > m_width)
			width = m_width - top_right_x;
		if (top_right_y + height > m_height)
			height = m_height - top_right_y;

		auto* video_memory_u8 = reinterpret_cast<uint8_t*>(m_video_memory_vaddr);
		auto* video_buffer_u8 = reinterpret_cast<uint8_t*>(m_video_buffer->vaddr());

		for (uint32_t row = top_right_y; row < top_right_y + height; row++)
		{
			for (uint32_t idx = top_right_x; idx < top_right_x + width; idx++)
			{
				video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 0] = video_buffer_u8[(row * m_width + idx) * (BANAN_FB_BPP / 8) + 0];
				video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 1] = video_buffer_u8[(row * m_width + idx) * (BANAN_FB_BPP / 8) + 1];
				video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 2] = video_buffer_u8[(row * m_width + idx) * (BANAN_FB_BPP / 8) + 2];
			}
		}
	}

	class FramebufferMemoryRegion : public MemoryRegion
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<FramebufferMemoryRegion>> create(PageTable& page_table, size_t size, AddressRange address_range, MemoryRegion::Type region_type, PageTable::flags_t page_flags, BAN::RefPtr<FramebufferDevice> framebuffer)
		{
			auto* region_ptr = new FramebufferMemoryRegion(page_table, size, region_type, page_flags, framebuffer);
			if (region_ptr == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			auto region = BAN::UniqPtr<FramebufferMemoryRegion>::adopt(region_ptr);

			TRY(region->initialize(address_range));

			return region;
		}

		~FramebufferMemoryRegion()
		{
			m_framebuffer->sync_pixels_full();
		}

		virtual BAN::ErrorOr<void> msync(vaddr_t vaddr, size_t size, int flags) override
		{
			if (flags != MS_SYNC)
				return BAN::Error::from_errno(ENOTSUP);

			if (vaddr < m_vaddr)
				vaddr = m_vaddr;
			if (vaddr + size > m_vaddr + m_size)
				size = (vaddr - m_vaddr) + m_size;

			m_framebuffer->sync_pixels_linear(
				(vaddr - m_vaddr) / (BANAN_FB_BPP / 8),
				BAN::Math::div_round_up<uint32_t>((vaddr % (BANAN_FB_BPP / 8)) + size, (BANAN_FB_BPP / 8))
			);

			return {};
		}

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) override
		{
			(void)new_page_table;
			return BAN::Error::from_errno(ENOTSUP);
		}

	protected:
		// Returns error if no memory was available
		// Returns true if page was succesfully allocated
		// Returns false if page was already allocated
		virtual BAN::ErrorOr<bool> allocate_page_containing_impl(vaddr_t vaddr) override
		{
			vaddr &= PAGE_ADDR_MASK;
			if (m_page_table.physical_address_of(vaddr))
				return false;
			
			paddr_t paddr = PageTable::kernel().physical_address_of(m_framebuffer->m_video_buffer->vaddr() + (vaddr - m_vaddr));
			m_page_table.map_page_at(paddr, vaddr, m_flags);

			return true;
		}

	private:
		FramebufferMemoryRegion(PageTable& page_table, size_t size, MemoryRegion::Type region_type, PageTable::flags_t page_flags, BAN::RefPtr<FramebufferDevice> framebuffer)
			: MemoryRegion(page_table, size, region_type, page_flags)
			, m_framebuffer(framebuffer)
		{ }

	private:
		BAN::RefPtr<FramebufferDevice> m_framebuffer;
	};

	BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> FramebufferDevice::mmap_region(PageTable& page_table, off_t offset, size_t len, AddressRange address_range, MemoryRegion::Type region_type, PageTable::flags_t page_flags)
	{
		if (offset != 0)
			return BAN::Error::from_errno(EINVAL);
		if (len > m_video_buffer->size())
			return BAN::Error::from_errno(EINVAL);
		if (region_type != MemoryRegion::Type::SHARED)
			return BAN::Error::from_errno(EINVAL);

		auto region = TRY(FramebufferMemoryRegion::create(page_table, len, address_range, region_type, page_flags, this));
		return BAN::UniqPtr<MemoryRegion>(BAN::move(region));
	}

}
