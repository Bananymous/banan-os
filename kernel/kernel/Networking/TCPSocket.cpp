#include <kernel/LockGuard.h>
#include <kernel/Networking/TCPSocket.h>
#include <kernel/Random.h>
#include <kernel/Timer/Timer.h>

#include <netinet/in.h>

#define DEBUG_TCP 0

namespace Kernel
{

	enum TCPOption : uint8_t
	{
		End					= 0x00,
		NOP					= 0x01,
		MaximumSeqmentSize	= 0x02,
		WindowScale			= 0x03,
	};

	static constexpr size_t s_window_buffer_size = 15 * PAGE_SIZE;
	static_assert(s_window_buffer_size <= UINT16_MAX);

	BAN::ErrorOr<BAN::RefPtr<TCPSocket>> TCPSocket::create(NetworkLayer& network_layer, ino_t ino, const TmpInodeInfo& inode_info)
	{
		auto* socket_ptr = new TCPSocket(network_layer, ino, inode_info);
		if (socket_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto socket = BAN::RefPtr<TCPSocket>::adopt(socket_ptr);
		socket->m_recv_window.window = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(vaddr_t)0,
			s_window_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		socket->m_send_window.window = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(vaddr_t)0,
			s_window_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		socket->m_recv_window.size = socket->m_recv_window.window->size();
		socket->m_recv_window.scale = 0;
		socket->m_process = Process::create_kernel(
			[](void* socket_ptr)
			{
				reinterpret_cast<TCPSocket*>(socket_ptr)->process_task();
			}, socket.ptr()
		);
		return socket;
	}

	TCPSocket::TCPSocket(NetworkLayer& network_layer, ino_t ino, const TmpInodeInfo& inode_info)
		: NetworkSocket(network_layer, ino, inode_info)
	{
		m_send_window.start_seq = Random::get_u32() & 0x7FFFFFFF;
		m_send_window.ack_number = m_send_window.start_seq;
		m_send_window.current_seq = m_send_window.start_seq;
	}

	TCPSocket::~TCPSocket()
	{
		ASSERT(!is_bound());
		ASSERT(m_process == nullptr);
		dprintln_if(DEBUG_TCP, "socket destroyed");
	}

	void TCPSocket::on_close_impl()
	{
		LockGuard _(m_lock);

		if (!is_bound())
			return;

		switch (m_state)
		{
			case State::Established:
				break;
			case State::SynSent:
				set_connection_as_closed();
				// fall through
			case State::SynReceived:
			case State::FinWait1:
			case State::FinWait2:
			case State::CloseWait:
			case State::Closing:
			case State::TimeWait:
			case State::LastAck:
				return;
			case State::Closed:		ASSERT_NOT_REACHED();
			case State::Listen:		ASSERT_NOT_REACHED();
		}

		ASSERT(m_connection_info.has_value());
		auto* target_address = reinterpret_cast<const sockaddr*>(&m_connection_info->address);
		auto target_address_len = m_connection_info->address_len;

		m_state = State::FinWait1;
		if (auto ret = m_network_layer.sendto(*this, {}, target_address, target_address_len); ret.is_error())
			dwarnln("{}", ret.error());

		dprintln_if(DEBUG_TCP, "Initiated close");
	}

	BAN::ErrorOr<void> TCPSocket::connect_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len > (socklen_t)sizeof(sockaddr_storage))
			address_len = sizeof(sockaddr_storage);

		LockGuard _(m_lock);

		ASSERT(!m_connection_info.has_value());

		switch (m_state)
		{
			case State::Closed:
				break;
			case State::SynSent:
			case State::SynReceived:
				return BAN::Error::from_errno(EALREADY);
			case State::Established:
			case State::FinWait1:
			case State::FinWait2:
			case State::CloseWait:
			case State::Closing:
			case State::LastAck:
			case State::TimeWait:
				return BAN::Error::from_errno(EISCONN);
			case State::Listen:
				return BAN::Error::from_errno(EOPNOTSUPP);
		};

		if (!is_bound())
			TRY(m_network_layer.bind_socket_to_unused(this, address, address_len));

		m_connection_info.emplace(sockaddr_storage {}, address_len);
		memcpy(&m_connection_info->address, address, address_len);

		m_recv_window.mss = m_interface->payload_mtu() - m_network_layer.header_size();

		TRY(m_network_layer.sendto(*this, {}, address, address_len));
		ASSERT(m_state == State::SynSent);
		dprintln_if(DEBUG_TCP, "Sent SYN");

		uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + 5000;
		while (m_state != State::Established)
		{
			LockFreeGuard free(m_lock);
			if (SystemTimer::get().ms_since_boot() >= wake_time_ms)
				return BAN::Error::from_errno(ECONNREFUSED);
			TRY(Thread::current().block_or_eintr_or_waketime(m_semaphore, wake_time_ms, true));
		}

		return {};
	}

	template<size_t Off, TCPOption Op>
	static void add_tcp_header_option(TCPHeader& header, uint32_t value)
	{
		if constexpr(Op == TCPOption::MaximumSeqmentSize)
		{
			header.options[Off + 0] = Op;
			header.options[Off + 1] = 0x04;
			header.options[Off + 2] = value >> 8;
			header.options[Off + 3] = value;
		}
		else if constexpr(Op == TCPOption::WindowScale)
		{
			header.options[Off + 0] = Op;
			header.options[Off + 1] = 0x03;
			header.options[Off + 2] = value;
		}
	}

	struct ParsedTCPOptions
	{
		BAN::Optional<uint16_t> maximum_seqment_size;
		BAN::Optional<uint8_t> window_scale;
	};
	static ParsedTCPOptions parse_tcp_options(const TCPHeader& header)
	{
		ParsedTCPOptions result;

		for (size_t i = 0; i < header.data_offset * sizeof(uint32_t) - sizeof(TCPHeader) - 1; i++)
		{
			if (header.options[i] == TCPOption::End)
				break;
			if (header.options[i] == TCPOption::NOP)
				continue;
			if (header.options[i] == TCPOption::MaximumSeqmentSize)
				result.maximum_seqment_size = BAN::host_to_network_endian(*reinterpret_cast<const uint16_t*>(&header.options[i + 2]));
			if (header.options[i] == TCPOption::WindowScale)
				result.window_scale = header.options[i + 2];
			if (header.options[i + 1] == 0)
				break;
			i += header.options[i + 1] - 1;
		}

		return result;
	}

	void TCPSocket::add_protocol_header(BAN::ByteSpan packet, uint16_t dst_port, PseudoHeader pseudo_header)
	{
		auto& header = packet.as<TCPHeader>();
		memset(&header, 0, sizeof(TCPHeader));
		memset(header.options, TCPOption::End, m_tcp_options_bytes);

		header.dst_port = dst_port;
		header.src_port = m_port;
		header.seq_number = m_send_window.current_seq;
		header.ack_number = m_recv_window.ack_number.load();
		header.data_offset = (sizeof(TCPHeader) + m_tcp_options_bytes) / sizeof(uint32_t);
		header.window_size = m_recv_window.window->size();

		switch (m_state)
		{
			case State::Closed:
			{
				LockGuard _(m_lock);
				header.syn = 1;
				add_tcp_header_option<0, TCPOption::MaximumSeqmentSize>(header, m_recv_window.mss);
				add_tcp_header_option<4, TCPOption::WindowScale>(header, m_recv_window.scale);
				m_state = State::SynSent;
				break;
			}
			case State::SynSent:
				header.ack = 1;
				break;
			case State::SynReceived:
				header.ack = 1;
				m_state = State::Established;
				break;
			case State::Established:
				header.ack = 1;
				break;
			case State::CloseWait:
			{
				LockGuard _(m_lock);
				header.ack = 1;
				header.fin = 1;
				header.ack_number = header.ack_number + 1;
				m_state = State::LastAck;
				dprintln_if(DEBUG_TCP, "Waiting for last ack");
				break;
			}
			case State::FinWait1:
			{
				LockGuard _(m_lock);
				header.ack = 1;
				header.fin = 1;
				m_state = State::FinWait2;
				break;
			}
			case State::FinWait2:
			{
				LockGuard _(m_lock);
				header.ack = 1;
				header.seq_number = header.seq_number + 1;
				header.ack_number = header.ack_number + 1;
				m_state = State::TimeWait;
				m_time_wait_start_ms = SystemTimer::get().ms_since_boot();
				break;
			}
			case State::Listen:		ASSERT_NOT_REACHED();
			case State::Closing:	ASSERT_NOT_REACHED();
			case State::LastAck:	ASSERT_NOT_REACHED();
			case State::TimeWait:	ASSERT_NOT_REACHED();
		}

		pseudo_header.extra = packet.size();
		header.checksum = calculate_internet_checksum(packet, pseudo_header);
	}

	void TCPSocket::receive_packet(BAN::ConstByteSpan buffer, const sockaddr_storage& sender)
	{
		{
			uint16_t checksum = 0;

			if (sender.ss_family == AF_INET)
			{
				auto& sockaddr_in = *reinterpret_cast<const struct sockaddr_in*>(&sender);
				checksum = calculate_internet_checksum(buffer,
					PseudoHeader {
						.src_ipv4 = BAN::IPv4Address(sockaddr_in.sin_addr.s_addr),
						.dst_ipv4 = m_interface->get_ipv4_address(),
						.protocol = NetworkProtocol::TCP,
						.extra = buffer.size()
					}
				);
			}
			else
			{
				dwarnln("No tcp checksum validation for socket family {}", sender.ss_family);
				return;
			}

			if (checksum != 0)
			{
				dprintln("Checksum does not match");
				return;
			}
		}

		auto& header = buffer.as<const TCPHeader>();

		m_send_window.size = header.window_size;

		auto payload = buffer.slice(header.data_offset * sizeof(uint32_t));

		switch (m_state)
		{
			case State::Closed:
				break;
			case State::SynSent:
			{
				if (!header.ack || !header.syn)
					break;

				LockGuard _(m_lock);

				auto options = parse_tcp_options(header);
				if (options.maximum_seqment_size.has_value())
					m_send_window.mss = *options.maximum_seqment_size;
				if (options.window_scale.has_value())
					m_send_window.scale = *options.window_scale;
				else
					m_recv_window.scale = 0;

				m_send_window.start_seq = m_send_window.start_seq + 1;
				m_send_window.ack_number = m_send_window.start_seq;
				m_send_window.current_seq = m_send_window.start_seq;

				m_recv_window.start_seq = header.seq_number + 1;
				m_recv_window.ack_number = m_recv_window.start_seq;

				dprintln_if(DEBUG_TCP, "Got SYN/ACK");

				m_should_ack = true;
				m_state = State::SynReceived;
				break;
			}
			case State::FinWait2:
				if (!header.ack)
					break;
				if (header.fin)
					m_should_ack = true;
				// fall through
			case State::TimeWait:
			case State::CloseWait:
			case State::Established:
			{
				if (!header.ack)
					break;

				LockGuard _(m_lock);
				if (header.fin)
				{
					if (m_recv_window.start_seq + m_recv_window.data_size != header.seq_number)
						dprintln_if(DEBUG_TCP, "Got FIN, but missing packets");
					else
					{
						m_should_ack = true;
						m_state = State::CloseWait;
						dprintln_if(DEBUG_TCP, "Got FIN");
					}
					break;
				}
				if (header.ack_number > m_send_window.ack_number)
					m_send_window.ack_number = header.ack_number;
				if (payload.size() > 0)
				{
					if (header.seq_number != m_recv_window.start_seq + m_recv_window.data_size)
					{
						dprintln_if(DEBUG_TCP, "Missing packet");
						break;
					}

					if (m_recv_window.data_size + payload.size() > m_recv_window.window->size())
					{
						dwarnln("Cannot fit received bytes to window");
						break;
					}

					auto* buffer = reinterpret_cast<uint8_t*>(m_recv_window.window->vaddr());
					memcpy(buffer + m_recv_window.data_size, payload.data(), payload.size());
					m_recv_window.data_size += payload.size();

					m_should_ack = true;

					dprintln_if(DEBUG_TCP, "Received {} bytes", payload.size());
				}
				break;
			}
			case State::LastAck:
				if (!header.ack)
					break;
				set_connection_as_closed();
				dprintln_if(DEBUG_TCP, "Got final ACK");
				break;
			case State::Listen:			ASSERT_NOT_REACHED();
			case State::SynReceived:	ASSERT_NOT_REACHED();
			case State::FinWait1:		ASSERT_NOT_REACHED();
			case State::Closing:		ASSERT_NOT_REACHED();
		}

		m_semaphore.unblock();
	}

	void TCPSocket::set_connection_as_closed()
	{
		if (is_bound())
		{
			m_network_layer.unbind_socket(this, m_port);
			m_interface = nullptr;
			m_port = PORT_NONE;
			dprintln_if(DEBUG_TCP, "Socket unbound");
		}

		m_process = nullptr;
	}

	void TCPSocket::process_task()
	{
		// FIXME: this should be dynamic
		static constexpr uint32_t retransmit_timeout_ms = 100;

		BAN::RefPtr<TCPSocket> keep_alive = this;

		while (m_process)
		{
			uint64_t current_ms = SystemTimer::get().ms_since_boot();

			if (m_state == State::TimeWait && current_ms >= m_time_wait_start_ms + 6'000)
				set_connection_as_closed();

			{
				LockGuard _(m_lock);

				if (m_should_ack || m_recv_window.start_seq + m_recv_window.data_size != m_recv_window.ack_number)
				{
					m_should_ack = false;

					ASSERT(m_connection_info.has_value());
					auto* target_address = reinterpret_cast<const sockaddr*>(&m_connection_info->address);
					auto target_address_len = m_connection_info->address_len;

					m_recv_window.ack_number = m_recv_window.start_seq + m_recv_window.data_size;
					if (auto ret = m_network_layer.sendto(*this, {}, target_address, target_address_len); ret.is_error())
						dwarnln("{}", ret.error());

					continue;
				}

				bool is_send_open = false;
				switch (m_state)
				{
					case State::Listen:
					case State::Established:
					case State::CloseWait:
					case State::LastAck:
						is_send_open = true;
						break;
					case State::SynSent:
					case State::SynReceived:
					case State::FinWait1:
					case State::FinWait2:
					case State::TimeWait:
					case State::Closed:
						is_send_open = false;
						break;
					case State::Closing:	ASSERT_NOT_REACHED();
				}

				if (is_send_open && m_send_window.ack_number > m_send_window.start_seq)
				{
					uint32_t acknowledged_bytes = m_send_window.ack_number - m_send_window.start_seq;
					ASSERT(acknowledged_bytes <= m_send_window.data_size);

					m_send_window.data_size -= acknowledged_bytes;
					m_send_window.start_seq += acknowledged_bytes;

					if (m_send_window.data_size > 0)
					{
						auto* send_buffer = reinterpret_cast<uint8_t*>(m_send_window.window->vaddr());
						memmove(send_buffer, send_buffer + acknowledged_bytes, m_send_window.data_size);
					}
					else
					{
						m_send_window.send_time_ms = 0;
					}

					dprintln_if(DEBUG_TCP, "Target acknowledged {} bytes", acknowledged_bytes);

					continue;
				}

				if (is_send_open && m_send_window.data_size > 0 && current_ms >= m_send_window.send_time_ms + retransmit_timeout_ms)
				{
					ASSERT(m_connection_info.has_value());
					auto* target_address = reinterpret_cast<const sockaddr*>(&m_connection_info->address);
					auto target_address_len = m_connection_info->address_len;

					const uint32_t total_send = BAN::Math::min<uint32_t>(m_send_window.data_size, m_send_window.size << m_send_window.scale);

					m_send_window.current_seq = m_send_window.start_seq;

					auto* send_buffer = reinterpret_cast<const uint8_t*>(m_send_window.window->vaddr());
					for (uint32_t i = 0; i < total_send;)
					{
						uint32_t to_send = BAN::Math::min(total_send - i, m_send_window.mss);

						auto message = BAN::ConstByteSpan(send_buffer + i, to_send);

						if (auto ret = m_network_layer.sendto(*this, message, target_address, target_address_len); ret.is_error())
						{
							dwarnln("{}", ret.error());
							break;
						}

						dprintln_if(DEBUG_TCP, "Sent {} bytes", to_send);

						m_send_window.current_seq += to_send;
						i += to_send;
					}

					m_send_window.send_time_ms = current_ms;

					continue;
				}
			}

			m_semaphore.block_with_wake_time(current_ms + retransmit_timeout_ms);
		}

		m_semaphore.unblock();
	}

	BAN::ErrorOr<size_t> TCPSocket::recvfrom_impl(BAN::ByteSpan buffer, sockaddr*, socklen_t*)
	{
		LockGuard _(m_lock);

		if (m_state == State::Closed)
			return BAN::Error::from_errno(ENOTCONN);

		while (m_recv_window.data_size == 0)
		{
			switch (m_state)
			{
				case State::SynSent:
				case State::SynReceived:
				case State::Established:
				case State::CloseWait:
				case State::Listen:
					break;
				case State::FinWait1:
				case State::FinWait2:
				case State::LastAck:
				case State::TimeWait:
					return BAN::Error::from_errno(ECONNRESET);
				case State::Closed:		ASSERT_NOT_REACHED();
				case State::Closing:	ASSERT_NOT_REACHED();
			};

			LockFreeGuard free(m_lock);
			TRY(Thread::current().block_or_eintr_indefinite(m_semaphore));
		}

		uint32_t to_recv = BAN::Math::min<uint32_t>(buffer.size(), m_recv_window.data_size);

		auto* recv_buffer = reinterpret_cast<uint8_t*>(m_recv_window.window->vaddr());
		memcpy(buffer.data(), recv_buffer, to_recv);

		m_recv_window.data_size -= to_recv;
		m_recv_window.start_seq += to_recv;
		if (m_recv_window.data_size > 0)
			memmove(recv_buffer, recv_buffer + to_recv, m_recv_window.data_size);

		return to_recv;
	}

	BAN::ErrorOr<size_t> TCPSocket::sendto_impl(BAN::ConstByteSpan message, const sockaddr* address, socklen_t)
	{
		if (address)
			return BAN::Error::from_errno(EISCONN);

		if (message.size() > m_send_window.window->size())
			return BAN::Error::from_errno(EMSGSIZE);

		LockGuard _(m_lock);

		if (m_state == State::Closed)
			return BAN::Error::from_errno(ENOTCONN);

		while (m_send_window.data_size + message.size() > m_send_window.window->size())
		{
			switch (m_state)
			{
				case State::SynSent:
				case State::SynReceived:
				case State::Established:
				case State::CloseWait:
				case State::Listen:
					break;
				case State::FinWait1:
				case State::FinWait2:
				case State::LastAck:
				case State::TimeWait:
					return BAN::Error::from_errno(ECONNRESET);
				case State::Closed:		ASSERT_NOT_REACHED();
				case State::Closing:	ASSERT_NOT_REACHED();
			};

			LockFreeGuard free(m_lock);
			TRY(Thread::current().block_or_eintr_indefinite(m_semaphore));
		}

		{
			auto* buffer = reinterpret_cast<uint8_t*>(m_send_window.window->vaddr());
			memcpy(buffer + m_send_window.data_size, message.data(), message.size());
			m_send_window.data_size += message.size();
		}

		uint32_t target_ack = m_send_window.start_seq + m_send_window.data_size;
		m_semaphore.unblock();

		while (m_send_window.start_seq < target_ack)
		{
			switch (m_state)
			{
				case State::SynSent:
				case State::SynReceived:
				case State::Established:
				case State::CloseWait:
				case State::Listen:
				case State::TimeWait:
				case State::FinWait1:
				case State::FinWait2:
					break;
				case State::LastAck:
					return BAN::Error::from_errno(ECONNRESET);
				case State::Closed:		ASSERT_NOT_REACHED();
				case State::Closing:	ASSERT_NOT_REACHED();
			};

			LockFreeGuard free(m_lock);
			TRY(Thread::current().block_or_eintr_indefinite(m_semaphore));
		}

		return message.size();
	}

}
