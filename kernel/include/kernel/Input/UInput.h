#pragma once

#include <kernel/Device/Device.h>
#include <kernel/Input/InputDevice.h>

namespace Kernel
{

	class UInputSlave;

	class UInputControl final : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UInputControl>> create(mode_t, uid_t, gid_t);

		BAN::StringView name() const override { return "uinput"_sv; }

	private:
		UInputControl(mode_t, uid_t, gid_t);

	private:
		BAN::ErrorOr<long> ioctl_impl(unsigned long, void*) override;

		bool can_read_impl()   const override { return false; }
		bool can_write_impl()  const override { return false; }
		bool has_error_impl()  const override { return false; }
		bool has_hungup_impl() const override { return false; }
	};



	class UInputMaster final : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UInputMaster>> create(BAN::RefPtr<UInputSlave>);

		BAN::StringView name() const override { return "<uintput master>"_sv; }

	private:
		UInputMaster(BAN::RefPtr<UInputSlave>);
		virtual ~UInputMaster();

	private:
		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		bool can_read_impl()   const override { return false; }
		bool can_write_impl()  const override { return true; }
		bool has_error_impl()  const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		BAN::RefPtr<UInputSlave> m_slave;
	};



	class UInputSlave final : public InputDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<UInputSlave>> create(InputDevice::Type);

		BAN::ErrorOr<size_t> on_master_write(BAN::ConstByteSpan);

	private:
		UInputSlave(InputDevice::Type);
	};

}
