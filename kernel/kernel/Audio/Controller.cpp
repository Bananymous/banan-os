#include <kernel/Audio/AC97/Controller.h>
#include <kernel/Audio/Controller.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Lock/SpinLockAsMutex.h>

#include <sys/ioctl.h>
#include <sys/sysmacros.h>

namespace Kernel
{

	static BAN::Atomic<dev_t> s_next_audio_minor = 0;

	AudioController::AudioController()
		: CharacterDevice(0644, 0, 0)
		, m_rdev(makedev(DeviceNumber::AudioController, s_next_audio_minor++))
	{
		char* ptr = m_name;
		BAN::Formatter::print([&ptr](char c) { *ptr++ = c; }, "audio{}", minor(m_rdev));
	}

	BAN::ErrorOr<BAN::RefPtr<AudioController>> AudioController::create(PCI::Device& pci_device)
	{
		switch (pci_device.subclass())
		{
			case 0x01:
				// We should confirm that the card is actually AC97 but I'm trusting osdev wiki on this one
				// > you can probably expect that every sound card with subclass 0x01 is sound card compatibile with AC97
				if (auto ret = AC97AudioController::create(pci_device); !ret.is_error())
				{
					DevFileSystem::get().add_device(ret.value());
					return BAN::RefPtr<AudioController>(ret.release_value());
				}
				else
				{
					dwarnln("Failed to initialize AC97: {}", ret.error());
					return ret.release_error();
				}
			default:
				dprintln("Unsupported Sound card (PCI {2H}:{2H}:{2H})",
					pci_device.class_code(),
					pci_device.subclass(),
					pci_device.prog_if()
				);
				return BAN::Error::from_errno(ENOTSUP);
		}
	}


	BAN::ErrorOr<size_t> AudioController::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		SpinLockGuard lock_guard(m_spinlock);

		while (m_sample_data_size >= m_sample_data_capacity)
		{
			SpinLockGuardAsMutex smutex(lock_guard);
			TRY(Thread::current().block_or_eintr_indefinite(m_sample_data_blocker, &smutex));
		}

		size_t nwritten = 0;
		while (nwritten < buffer.size())
		{
			if (m_sample_data_size >= m_sample_data_capacity)
				break;

			const size_t max_memcpy = BAN::Math::min(m_sample_data_capacity - m_sample_data_size, m_sample_data_capacity - m_sample_data_head);
			const size_t to_copy = BAN::Math::min(buffer.size() - nwritten, max_memcpy);
			memcpy(m_sample_data + m_sample_data_head, buffer.data() + nwritten, to_copy);

			nwritten += to_copy;
			m_sample_data_head = (m_sample_data_head + to_copy) % m_sample_data_capacity;
			m_sample_data_size += to_copy;
		}

		handle_new_data();

		return nwritten;
	}

	BAN::ErrorOr<long> AudioController::ioctl_impl(int cmd, void* arg)
	{
		switch (cmd)
		{
			case SND_GET_CHANNELS:
				*static_cast<uint32_t*>(arg) = get_channels();
				return 0;
			case SND_GET_SAMPLE_RATE:
				*static_cast<uint32_t*>(arg) = get_sample_rate();
				return 0;
			case SND_RESET_BUFFER:
			case SND_GET_BUFFERSZ:
			{
				SpinLockGuard _(m_spinlock);
				*static_cast<uint32_t*>(arg) = m_sample_data_size;
				if (cmd == SND_RESET_BUFFER)
					m_sample_data_size = 0;
				return 0;
			}
		}

		return CharacterDevice::ioctl_impl(cmd, arg);
	}

}
