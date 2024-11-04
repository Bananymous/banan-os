#pragma once

#include <BAN/WeakPtr.h>
#include <kernel/Terminal/TTY.h>

namespace Kernel
{

	class PseudoTerminalSlave;

	class PseudoTerminalMaster final : public CharacterDevice, public BAN::Weakable<PseudoTerminalMaster>
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<PseudoTerminalMaster>> create(mode_t, uid_t, gid_t);

		dev_t rdev() const override { return m_rdev; }
		BAN::StringView name() const override { return "<ptmx>"_sv; }

		BAN::ErrorOr<BAN::RefPtr<PseudoTerminalSlave>> slave();

		BAN::ErrorOr<BAN::String> ptsname();

		void putchar(uint8_t ch);

	protected:
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		bool can_read_impl() const override { SpinLockGuard _(m_buffer_lock); return m_buffer_size > 0; }
		bool can_write_impl() const override { SpinLockGuard _(m_buffer_lock); return m_buffer_size < m_buffer->size(); }
		bool has_error_impl() const override { return false; }

	private:
		PseudoTerminalMaster(BAN::UniqPtr<VirtualRange>, mode_t, uid_t, gid_t);
		~PseudoTerminalMaster();

	private:
		BAN::WeakPtr<PseudoTerminalSlave> m_slave;

		mutable SpinLock m_buffer_lock;
		ThreadBlocker m_buffer_blocker;
		BAN::UniqPtr<VirtualRange> m_buffer;
		size_t m_buffer_tail { 0 };
		size_t m_buffer_size { 0 };

		const dev_t m_rdev;

		friend class BAN::RefPtr<PseudoTerminalMaster>;
	};

	class PseudoTerminalSlave final : public TTY, public BAN::Weakable<PseudoTerminalSlave>
	{
	public:
		BAN::StringView name() const override { return m_name; }

		uint32_t height() const override { return m_height; }
		uint32_t width() const override { return m_width; }

		void clear() override;

	protected:
		void putchar_impl(uint8_t ch) override;

	private:
		PseudoTerminalSlave(BAN::String&& name, uint32_t number, mode_t, uid_t, gid_t);
		~PseudoTerminalSlave();

	private:
		const BAN::String m_name;
		const uint32_t m_number;

		BAN::WeakPtr<PseudoTerminalMaster> m_master;
		uint32_t m_width { 0 };
		uint32_t m_height { 0 };

		friend class PseudoTerminalMaster;
		friend class BAN::RefPtr<PseudoTerminalSlave>;
	};

}
