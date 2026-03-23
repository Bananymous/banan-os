#include <kernel/Audio/AC97/Controller.h>
#include <kernel/Audio/Controller.h>
#include <kernel/Audio/HDAudio/Controller.h>
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

	BAN::ErrorOr<void> AudioController::create(PCI::Device& pci_device)
	{
		switch (pci_device.subclass())
		{
			case 0x01:
				// We should confirm that the card is actually AC97 but I'm trusting osdev wiki on this one
				// > you can probably expect that every sound card with subclass 0x01 is sound card compatibile with AC97
				if (auto ret = AC97AudioController::create(pci_device); ret.is_error())
				{
					dwarnln("Failed to initialize AC97: {}", ret.error());
					return ret.release_error();
				}
				break;
			case 0x03:
				if (auto ret = HDAudioController::create(pci_device); ret.is_error())
				{
					dwarnln("Failed to initialize Intel HDA: {}", ret.error());
					return ret.release_error();
				}
				break;
			default:
				dprintln("Unsupported Sound card (PCI {2H}:{2H}:{2H})",
					pci_device.class_code(),
					pci_device.subclass(),
					pci_device.prog_if()
				);
				return BAN::Error::from_errno(ENOTSUP);
		}

		return {};
	}

	BAN::ErrorOr<void> AudioController::initialize()
	{
		m_sample_data = TRY(ByteRingBuffer::create(m_sample_data_capacity));
		return {};
	}

	BAN::ErrorOr<size_t> AudioController::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		SpinLockGuard lock_guard(m_spinlock);

		while (m_sample_data->full())
		{
			SpinLockGuardAsMutex smutex(lock_guard);
			TRY(Thread::current().block_or_eintr_indefinite(m_sample_data_blocker, &smutex));
		}

		const size_t to_copy = BAN::Math::min(buffer.size(), m_sample_data->free());
		m_sample_data->push(buffer.slice(0, to_copy));

		handle_new_data();

		return to_copy;
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
				*static_cast<uint32_t*>(arg) = m_sample_data->size();
				if (cmd == SND_RESET_BUFFER)
					m_sample_data->pop(m_sample_data->size());
				return 0;
			}
			case SND_GET_TOTAL_PINS:
				*static_cast<uint32_t*>(arg) = get_total_pins();
				return 0;
			case SND_GET_PIN:
				*static_cast<uint32_t*>(arg) = get_current_pin();
				return 0;
			case SND_SET_PIN:
				TRY(set_current_pin(*static_cast<uint32_t*>(arg)));
				return 0;
		}

		return CharacterDevice::ioctl_impl(cmd, arg);
	}

}
