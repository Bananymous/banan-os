#include <kernel/Device/Device.h>

namespace Kernel
{

	class DebugDevice : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<DebugDevice>> create(mode_t, uid_t, gid_t);

		virtual dev_t rdev() const override { return m_rdev; }

		virtual BAN::StringView name() const override { return "debug"sv; }

	protected:
		DebugDevice(mode_t mode, uid_t uid, gid_t gid, dev_t rdev)
			: CharacterDevice(mode, uid, gid)
			, m_rdev(rdev)
		{ }

		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override { return 0; }
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan buffer) override;

		virtual bool can_read_impl() const override { return false; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }

	private:
		const dev_t m_rdev;
	};

}
