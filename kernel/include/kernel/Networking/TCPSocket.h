#pragma once

#include <BAN/Endianness.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/Process.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	struct TCPHeader
	{
		BAN::NetworkEndian<uint16_t>	src_port		{ 0 };
		BAN::NetworkEndian<uint16_t>	dst_port		{ 0 };
		BAN::NetworkEndian<uint32_t>	seq_number		{ 0 };
		BAN::NetworkEndian<uint32_t>	ack_number		{ 0 };
		uint8_t							reserved	: 4	{ 0 };
		uint8_t							data_offset	: 4	{ 0 };
		uint8_t							fin			: 1	{ 0 };
		uint8_t							syn			: 1	{ 0 };
		uint8_t							rst			: 1	{ 0 };
		uint8_t							psh			: 1	{ 0 };
		uint8_t							ack			: 1	{ 0 };
		uint8_t							urg			: 1	{ 0 };
		uint8_t							ece			: 1	{ 0 };
		uint8_t							cwr			: 1	{ 0 };
		BAN::NetworkEndian<uint16_t>	window_size		{ 0 };
		BAN::NetworkEndian<uint16_t>	checksum		{ 0 };
		BAN::NetworkEndian<uint16_t>	urgent_pointer	{ 0 };
		uint8_t							options[0];
	};
	static_assert(sizeof(TCPHeader) == 20);

	class TCPSocket final : public NetworkSocket
	{
	public:
		static constexpr size_t m_tcp_options_bytes = 4;

	public:
		static BAN::ErrorOr<BAN::RefPtr<TCPSocket>> create(NetworkLayer&, ino_t, const TmpInodeInfo&);
		~TCPSocket();

		virtual NetworkProtocol protocol() const override { return NetworkProtocol::TCP; }

		virtual size_t protocol_header_size() const override { return sizeof(TCPHeader) + m_tcp_options_bytes; }
		virtual void add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader) override;

	protected:
		virtual void on_close_impl() override;

		virtual BAN::ErrorOr<void> connect_impl(const sockaddr*, socklen_t) override;

		virtual void receive_packet(BAN::ConstByteSpan, const sockaddr_storage& sender) override;

		virtual BAN::ErrorOr<size_t> sendto_impl(BAN::ConstByteSpan message, const sockaddr* address, socklen_t address_len) override;
		virtual BAN::ErrorOr<size_t> recvfrom_impl(BAN::ByteSpan message, sockaddr* address, socklen_t* address_len) override;

		virtual bool can_read_impl() const override { return m_recv_window.data_size; }
		virtual bool can_write_impl() const override { return m_state == State::Established; }
		virtual bool has_error_impl() const override { return m_state != State::Established && m_state != State::Listen && m_state != State::SynSent && m_state != State::SynReceived; }

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
			BAN::UniqPtr<VirtualRange>	buffer;
		};

		struct SendWindowInfo
		{
			uint32_t					mss				{ 0 }; // maximum segment size
			uint16_t					non_scaled_size	{ 0 }; // window size without scaling
			uint8_t						scale			{ 0 }; // window scale
			uint32_t 					scaled_size() const { return (uint32_t)non_scaled_size << scale; }

			uint32_t					start_seq		{ 0 }; // sequence number of first byte in buffer
			uint32_t					current_seq		{ 0 }; // sequence number of next send
			uint32_t					current_ack		{ 0 }; // sequence number aknowledged by connection

			uint64_t					last_send_ms	{ 0 }; // last send time, used for retransmission timeout

			bool						has_ghost_byte { false };

			uint32_t					data_size		{ 0 }; // number of bytes in this buffer
			BAN::UniqPtr<VirtualRange>	buffer;
		};

	private:
		TCPSocket(NetworkLayer&, ino_t, const TmpInodeInfo&);
		void process_task();

		void set_connection_as_closed();

	private:
		State m_state = State::Closed;

		Process* m_process { nullptr };

		uint64_t m_time_wait_start_ms { 0 };

		Semaphore m_semaphore;

		BAN::Atomic<bool> m_should_ack { false };

		RecvWindowInfo m_recv_window;
		SendWindowInfo m_send_window;

		struct ConnectionInfo
		{
			sockaddr_storage	address;
			socklen_t			address_len;
		};
		BAN::Optional<ConnectionInfo> m_connection_info;
	};

}
