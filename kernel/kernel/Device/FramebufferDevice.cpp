#include <kernel/BootInfo.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Memory/Heap.h>

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
			BAN::Math::div_round_up<size_t>(m_width * m_height * 3, PAGE_SIZE) * PAGE_SIZE, // Internally we hold 24 bpp buffer
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));

		return {};
	}

	BAN::ErrorOr<size_t> FramebufferDevice::read_impl(off_t offset, BAN::ByteSpan buffer)
	{
		if (offset < 0)
			return BAN::Error::from_errno(EINVAL);
		if ((size_t)offset >= m_width * m_height * 3)
			return 0;
		
		size_t bytes_to_copy = BAN::Math::min<size_t>(m_width * m_height * 3 - offset, buffer.size());
		memcpy(buffer.data(), reinterpret_cast<void*>(m_video_buffer->vaddr() + offset), bytes_to_copy);

		return bytes_to_copy;
	}

	BAN::ErrorOr<size_t> FramebufferDevice::write_impl(off_t offset, BAN::ConstByteSpan buffer)
	{
		if (offset < 0)
			return BAN::Error::from_errno(EINVAL);
		if ((size_t)offset >= m_width * m_height * 3)
			return 0;
		
		size_t bytes_to_copy = BAN::Math::min<size_t>(m_width * m_height * 3 - offset, buffer.size());
		memcpy(reinterpret_cast<void*>(m_video_buffer->vaddr() + offset), buffer.data(), bytes_to_copy);

		uint32_t first_pixel = offset / 3;
		uint32_t pixel_count = BAN::Math::div_round_up<uint32_t>(bytes_to_copy + (offset % 3), 3);
		sync_pixels(first_pixel, pixel_count);

		return bytes_to_copy;
	}

	void FramebufferDevice::sync_pixels(uint32_t first_pixel, uint32_t pixel_count)
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

			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 0] = video_buffer_u8[(first_pixel + i) * 3 + 0];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 1] = video_buffer_u8[(first_pixel + i) * 3 + 1];
			video_memory_u8[(row * m_pitch) + (idx * m_bpp / 8) + 2] = video_buffer_u8[(first_pixel + i) * 3 + 2];
		}
	}

}