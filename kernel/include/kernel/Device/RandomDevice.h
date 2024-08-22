#include <kernel/Device/Device.h>

namespace Kernel
{

	class RandomDevice : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<RandomDevice>> create(mode_t, uid_t, gid_t);

		virtual dev_t rdev() const override { return m_rdev; }

		virtual BAN::StringView name() const override { return "random"_sv; }

	protected:
		RandomDevice(mode_t mode, uid_t uid, gid_t gid, dev_t rdev)
			: CharacterDevice(mode, uid, gid)
			, m_rdev(rdev)
		{ }

		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan buffer) override { return buffer.size(); };

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }

	private:
		const dev_t m_rdev;
	};

}
