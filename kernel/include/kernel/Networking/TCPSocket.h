#pragma once

#include <BAN/Endianness.h>
#include <BAN/Queue.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/Process.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	enum TCPFlags : uint8_t
	{
		FIN = 0x01,
		SYN = 0x02,
		RST = 0x04,
		PSH = 0x08,
		ACK = 0x10,
		URG = 0x20,
		ECE = 0x40,
		CWR = 0x80,
	};

	struct TCPHeader
	{
		BAN::NetworkEndian<uint16_t>	src_port		{ 0 };
		BAN::NetworkEndian<uint16_t>	dst_port		{ 0 };
		BAN::NetworkEndian<uint32_t>	seq_number		{ 0 };
		BAN::NetworkEndian<uint32_t>	ack_number		{ 0 };
		uint8_t							reserved	: 4	{ 0 };
		uint8_t							data_offset	: 4	{ 0 };
		uint8_t							flags			{   };
		BAN::NetworkEndian<uint16_t>	window_size		{ 0 };
		BAN::NetworkEndian<uint16_t>	checksum		{ 0 };
		BAN::NetworkEndian<uint16_t>	urgent_pointer	{ 0 };
		uint8_t							options[0];
	};
	static_assert(sizeof(TCPHeader) == 20);

	class TCPSocket final : public NetworkSocket
	{
	public:
		static constexpr size_t m_tcp_options_bytes = 8;

	public:
		static BAN::ErrorOr<BAN::RefPtr<TCPSocket>> create(NetworkLayer&, const Info&);
		~TCPSocket();

		virtual NetworkProtocol protocol() const override { return NetworkProtocol::TCP; }

		virtual size_t protocol_header_size() const override { return sizeof(TCPHeader) + m_tcp_options_bytes; }
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) override;

	protected:
		virtual BAN::ErrorOr<long> accept_impl(sockaddr*, socklen_t*, int) override;
		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<void> listen_impl(int) override;
		virtual BAN::ErrorOr<void> bind_impl(const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<size_t> sendto_impl(BAN::ConstByteSpan, const sockaddr*, socklen_t) override;
		virtual BAN::ErrorOr<size_t> recvfrom_impl(BAN::ByteSpan, sockaddr*, socklen_t*) override;

		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr* sender, socklen_t sender_len) override;

		virtual bool can_read_impl() const override;
		virtual bool can_write_impl() const override;
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override;

	private:
		enum class State
		{
			Closed = 0,
			Listen,
			SynSent,
			SynReceived,
			Established,
			FinWait1,
			FinWait2,
			CloseWait,
			Closing,
			LastAck,
			TimeWait,
		};

		struct RecvWindowInfo
		{
			uint32_t					start_seq	{ 0 }; // sequence number of first byte in buffer

			bool						has_ghost_byte { false };

			uint32_t					data_size	{ 0 }; // number of bytes in this buffer
			uint8_t						scale_shift	{ 0 }; // window scale
			BAN::UniqPtr<VirtualRange>	buffer;
		};

		struct SendWindowInfo
		{
			uint32_t					mss				{ 0 }; // maximum segment size
			uint16_t					non_scaled_size	{ 0 }; // window size without scaling
			uint8_t						scale_shift		{ 0 }; // window scale
			uint32_t 					scaled_size() const { return (uint32_t)non_scaled_size << scale_shift; }

			uint32_t					start_seq		{ 0 }; // sequence number of first byte in buffer
			uint32_t					current_seq		{ 0 }; // sequence number of next send
			uint32_t					current_ack		{ 0 }; // sequence number aknowledged by connection

			uint64_t					last_send_ms	{ 0 }; // last send time, used for retransmission timeout

			bool						has_ghost_byte { false };

			uint32_t					data_size		{ 0 }; // number of bytes in this buffer
			BAN::UniqPtr<VirtualRange>	buffer;
		};

		struct ConnectionInfo
		{
			sockaddr_storage	address;
			socklen_t			address_len;
			bool				has_window_scale;
		};

		struct PendingConnection
		{
			ConnectionInfo target;
			uint32_t target_start_seq;
		};

		struct ListenKey
		{
			ListenKey(const sockaddr* addr, socklen_t addr_len);
			ListenKey(BAN::IPv4Address addr, uint16_t port)
				: address(addr), port(port)
			{}
			bool operator==(const ListenKey& other)  const;
			BAN::IPv4Address address { 0 };
			uint16_t port            { 0 };
		};
		struct ListenKeyHash
		{
			BAN::hash_t operator()(ListenKey key) const;
		};

	private:
		TCPSocket(NetworkLayer&, const Info&);
		void process_task();

		void start_close_sequence();
		void set_connection_as_closed();

		void remove_listen_child(BAN::RefPtr<TCPSocket>);

		BAN::ErrorOr<size_t> return_with_maybe_zero();

	private:
		State m_state = State::Closed;

		State m_next_state		{ State::Closed };
		uint8_t m_next_flags	{ 0 };

		Process* m_process { nullptr };

		uint64_t m_time_wait_start_ms { 0 };

		ThreadBlocker m_thread_blocker;

		RecvWindowInfo m_recv_window;
		SendWindowInfo m_send_window;

		bool m_has_connected { false };
		bool m_has_sent_zero { false };

		BAN::Optional<ConnectionInfo> m_connection_info;
		BAN::Queue<PendingConnection> m_pending_connections;

		BAN::RefPtr<TCPSocket> m_listen_parent;
		BAN::HashMap<ListenKey, BAN::RefPtr<TCPSocket>, ListenKeyHash> m_listen_children;

		friend class BAN::RefPtr<TCPSocket>;
	};

}
