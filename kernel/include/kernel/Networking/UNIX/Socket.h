#pragma once

#include <BAN/Queue.h>
#include <BAN/WeakPtr.h>
#include <kernel/FS/Socket.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel
{

	class UnixDomainSocket final : public TmpInode, public BAN::Weakable<UnixDomainSocket>
	{
		BAN_NON_COPYABLE(UnixDomainSocket);
		BAN_NON_MOVABLE(UnixDomainSocket);

	public:
		static BAN::ErrorOr<BAN::RefPtr<UnixDomainSocket>> create(SocketType, ino_t, const TmpInodeInfo&);

	protected:
		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<long> accept_impl(sockaddr*, socklen_t*) override;
		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<void> listen_impl(int) override;
		virtual BAN::ErrorOr<void> bind_impl(const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<size_t> sendto_impl(BAN::ConstByteSpan, const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<size_t> recvfrom_impl(BAN::ByteSpan, sockaddr*, socklen_t*) override;

		virtual bool can_read_impl() const override;
		virtual bool can_write_impl() const override;
		virtual bool has_error_impl() const override { return false; }

	private:
		UnixDomainSocket(SocketType, ino_t, const TmpInodeInfo&);
		~UnixDomainSocket() { on_close_impl(); }

		BAN::ErrorOr<void> add_packet(BAN::ConstByteSpan);

		bool is_bound() const { return !m_bound_path.empty(); }
		bool is_bound_to_unused() const { return m_bound_path == "X"_sv; }

		bool is_streaming() const;

	private:
		struct ConnectionInfo
		{
			bool										listening { false };
			BAN::Atomic<bool>							connection_done { false };
			mutable BAN::Atomic<bool>					target_closed { false };
			BAN::WeakPtr<UnixDomainSocket>				connection;
			BAN::Queue<BAN::RefPtr<UnixDomainSocket>>	pending_connections;
			Semaphore									pending_semaphore;
			SpinLock									pending_lock;
		};

		struct ConnectionlessInfo
		{
			BAN::String peer_address;
		};

	private:
		const SocketType	m_socket_type;
		BAN::String			m_bound_path;

		BAN::Variant<ConnectionInfo, ConnectionlessInfo> m_info;

		BAN::CircularQueue<size_t, 128>	m_packet_sizes;
		size_t							m_packet_size_total { 0 };
		BAN::UniqPtr<VirtualRange>		m_packet_buffer;
		SpinLock						m_packet_lock;
		Semaphore						m_packet_semaphore;

		friend class BAN::RefPtr<UnixDomainSocket>;
	};

}
