#include <kernel/Lock/LockGuard.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/TCPSocket.h>
#include <kernel/Process.h>
#include <kernel/Random.h>
#include <kernel/Timer/Timer.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

namespace Kernel
{

	enum TCPOption : uint8_t
	{
		End					= 0x00,
		NOP					= 0x01,
		MaximumSeqmentSize	= 0x02,
		WindowScale			= 0x03,
	};

	static constexpr size_t s_recv_window_buffer_size = 16 * PAGE_SIZE;
	static constexpr size_t s_send_window_buffer_size = 16 * PAGE_SIZE;

	BAN::ErrorOr<BAN::RefPtr<TCPSocket>> TCPSocket::create(NetworkLayer& network_layer, const Info& info)
	{
		auto socket = TRY(BAN::RefPtr<TCPSocket>::create(network_layer, info));
		socket->m_recv_window.buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(vaddr_t)0,
			s_recv_window_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, false
		));
		socket->m_recv_window.scale_shift = PAGE_SIZE_SHIFT; // use PAGE_SIZE windows
		socket->m_send_window.buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(vaddr_t)0,
			s_send_window_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, false
		));
		socket->m_thread = TRY(Thread::create_kernel(
			[](void* socket_ptr)
			{
				reinterpret_cast<TCPSocket*>(socket_ptr)->process_task();
			}, socket.ptr()
		));
		TRY(Processor::scheduler().add_thread(socket->m_thread));
		// hack to keep socket alive until its process starts
		socket->ref();
		return socket;
	}

	TCPSocket::TCPSocket(NetworkLayer& network_layer, const Info& info)
		: NetworkSocket(network_layer, info)
	{
		m_send_window.start_seq = Random::get_u32() & 0x7FFFFFFF;
		m_send_window.current_seq = m_send_window.start_seq;
	}

	TCPSocket::~TCPSocket()
	{
		ASSERT(!is_bound());
		ASSERT(m_thread == nullptr);
		dprintln_if(DEBUG_TCP, "Socket destroyed");
	}

	BAN::ErrorOr<long> TCPSocket::accept_impl(sockaddr* address, socklen_t* address_len, int flags)
	{
		if (m_state != State::Listen)
			return BAN::Error::from_errno(EINVAL);

		while (m_pending_connections.empty())
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));

		auto connection = m_pending_connections.front();
		m_pending_connections.pop();

		auto listen_key = ListenKey(
			reinterpret_cast<const sockaddr*>(&connection.target.address),
			connection.target.address_len
		);
		if (auto it = m_listen_children.find(listen_key); it != m_listen_children.end())
			return BAN::Error::from_errno(ECONNABORTED);

		BAN::RefPtr<TCPSocket> return_inode;
		{
			auto return_inode_tmp = TRY(NetworkManager::get().create_socket(m_network_layer.domain(), Socket::Type::STREAM, mode().mode & ~Mode::TYPE_MASK, uid(), gid()));
			return_inode = static_cast<TCPSocket*>(return_inode_tmp.ptr());
		}

		return_inode->m_mutex.lock();
		return_inode->m_port = m_port;
		return_inode->m_interface = m_interface;
		return_inode->m_listen_parent = this;
		return_inode->m_connection_info.emplace(connection.target);
		return_inode->m_recv_window.start_seq = connection.target_start_seq;
		return_inode->m_next_flags = SYN | ACK;
		return_inode->m_next_state = State::SynReceived;
		return_inode->m_mutex.unlock();

		if (!return_inode->m_connection_info->has_window_scale)
			return_inode->m_recv_window.scale_shift = 0;

		TRY(m_listen_children.emplace(listen_key, return_inode));

		const uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + 5000;
		while (!return_inode->m_has_connected)
			TRY(Thread::current().block_or_eintr_or_waketime_ms(return_inode->m_thread_blocker, wake_time_ms, true, &m_mutex));

		if (address)
		{
			ASSERT(address_len);
			*address_len = BAN::Math::min(*address_len, connection.target.address_len);
			memcpy(address, &connection.target.address, *address_len);
		}

		return TRY(Process::current().open_inode(VirtualFileSystem::File(return_inode, "<tcp socket>"_sv), O_RDWR | flags));
	}

	BAN::ErrorOr<void> TCPSocket::connect_impl(const sockaddr* address, socklen_t address_len)
	{
		if (address_len > (socklen_t)sizeof(sockaddr_storage))
			address_len = sizeof(sockaddr_storage);

		LockGuard _(m_mutex);

		if (m_connection_info.has_value())
			return BAN::Error::from_errno(EISCONN);

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
			case State::Listen:
				return BAN::Error::from_errno(EISCONN);
		};

		if (!is_bound())
			TRY(m_network_layer.bind_socket_to_unused(this, address, address_len));

		m_connection_info.emplace(sockaddr_storage {}, address_len, true);
		memcpy(&m_connection_info->address, address, address_len);

		m_next_flags = SYN;
		TRY(m_network_layer.sendto(*this, {}, address, address_len));
		m_next_flags = 0;
		m_state = State::SynSent;

		const uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + 5000;
		while (!m_has_connected)
			TRY(Thread::current().block_or_eintr_or_waketime_ms(m_thread_blocker, wake_time_ms, true, &m_mutex));

		return {};
	}

	BAN::ErrorOr<void> TCPSocket::listen_impl(int backlog)
	{
		if (!is_bound())
			return BAN::Error::from_errno(EDESTADDRREQ);
		if (m_connection_info.has_value())
			return BAN::Error::from_errno(EINVAL);

		backlog = BAN::Math::clamp(backlog, 1, SOMAXCONN);
		TRY(m_pending_connections.reserve(backlog));
		m_state = State::Listen;

		return {};
	}

	BAN::ErrorOr<void> TCPSocket::bind_impl(const sockaddr* address, socklen_t address_len)
	{
		if (is_bound())
			return BAN::Error::from_errno(EINVAL);
		return m_network_layer.bind_socket_to_address(this, address, address_len);
	}

	BAN::ErrorOr<size_t> TCPSocket::recvmsg_impl(msghdr& message, int flags)
	{
		if (flags != 0)
		{
			dwarnln("TODO: recvmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (CMSG_FIRSTHDR(&message))
		{
			dwarnln("ignoring recvmsg control message");
			message.msg_controllen = 0;
		}

		if (!m_has_connected)
			return BAN::Error::from_errno(ENOTCONN);

		while (m_recv_window.data_size == 0)
		{
			if (m_state != State::Established)
				return return_with_maybe_zero();
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		message.msg_flags = 0;

		size_t total_recv = 0;
		for (int i = 0; i < message.msg_iovlen; i++)
		{
			auto* recv_buffer = reinterpret_cast<uint8_t*>(m_recv_window.buffer->vaddr());

			const size_t nrecv = BAN::Math::min<size_t>(message.msg_iov[i].iov_len, m_recv_window.data_size);
			memcpy(message.msg_iov[i].iov_base, recv_buffer, nrecv);

			total_recv += nrecv;
			m_recv_window.data_size -= nrecv;
			m_recv_window.start_seq += nrecv;
			if (m_recv_window.data_size == 0)
				break;

			// TODO: use circular buffer to avoid this
			memmove(recv_buffer, recv_buffer + nrecv, m_recv_window.data_size);
		}

		return total_recv;
	}

	BAN::ErrorOr<size_t> TCPSocket::sendmsg_impl(const msghdr& message, int flags)
	{
		if (flags != 0)
		{
			dwarnln("TODO: sendmsg with flags 0x{H}", flags);
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (CMSG_FIRSTHDR(&message))
			dwarnln("ignoring sendmsg control message");

		if (!m_has_connected)
			return BAN::Error::from_errno(ENOTCONN);

		while (m_send_window.data_size == m_send_window.buffer->size())
		{
			if (m_state != State::Established)
				return return_with_maybe_zero();
			TRY(Thread::current().block_or_eintr_indefinite(m_thread_blocker, &m_mutex));
		}

		size_t total_sent = 0;
		for (int i = 0; i < message.msg_iovlen; i++)
		{
			auto* send_buffer = reinterpret_cast<uint8_t*>(m_send_window.buffer->vaddr());

			const size_t nsend = BAN::Math::min<size_t>(message.msg_iov[i].iov_len, m_send_window.buffer->size() - m_send_window.data_size);
			memcpy(send_buffer + m_send_window.data_size, message.msg_iov[i].iov_base, nsend);

			total_sent += nsend;
			m_send_window.data_size += nsend;
			if (m_send_window.data_size == m_send_window.buffer->size())
				break;
		}

		m_thread_blocker.unblock();

		return total_sent;
	}

	BAN::ErrorOr<void> TCPSocket::getpeername_impl(sockaddr* address, socklen_t* address_len)
	{
		if (!m_has_connected && m_state != State::Established)
			return BAN::Error::from_errno(ENOTCONN);
		ASSERT(m_connection_info.has_value());
		const size_t to_copy = BAN::Math::min(m_connection_info->address_len, *address_len);
		memcpy(address, &m_connection_info->address, to_copy);
		*address_len = to_copy;
		return {};
	}

	BAN::ErrorOr<long> TCPSocket::ioctl_impl(int request, void* argument)
	{
		switch (request)
		{
			case FIONREAD:
				*static_cast<int*>(argument) = m_recv_window.data_size;
				return 0;
		}

		return NetworkSocket::ioctl_impl(request, argument);
	}

	bool TCPSocket::can_read_impl() const
	{
		if (m_has_connected && !m_has_sent_zero && m_state != State::Established && m_state != State::Listen)
			return true;
		if (m_state == State::Listen)
			return !m_pending_connections.empty();
		return m_recv_window.data_size > 0;
	}

	bool TCPSocket::can_write_impl() const
	{
		if (m_state != State::Established)
			return false;
		return m_send_window.data_size < m_send_window.buffer->size();
	}

	bool TCPSocket::has_hungup_impl() const
	{
		return m_has_connected && m_state != State::Established;
	}

	BAN::ErrorOr<size_t> TCPSocket::return_with_maybe_zero()
	{
		ASSERT(m_state != State::Established);
		if (!m_has_sent_zero)
		{
			m_has_sent_zero = true;
			return 0;
		}
		return BAN::Error::from_errno(ECONNRESET);
	}

	TCPSocket::ListenKey::ListenKey(const sockaddr* addr, socklen_t addr_len)
	{
		ASSERT(addr->sa_family == AF_INET);
		ASSERT(addr_len >= (socklen_t)sizeof(sockaddr_in));

		const auto* addr_in = reinterpret_cast<const sockaddr_in*>(addr);
		address = BAN::IPv4Address(addr_in->sin_addr.s_addr);
		port    = BAN::network_endian_to_host(addr_in->sin_port);
	}

	bool TCPSocket::ListenKey::operator==(const ListenKey& other) const
	{
		return address == other.address && port == other.port;
	}

	BAN::hash_t TCPSocket::ListenKeyHash::operator()(ListenKey key) const
	{
		return BAN::hash<BAN::IPv4Address>()(key.address) ^ BAN::hash<uint16_t>()(key.port);
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
		ASSERT(m_next_flags);
		ASSERT(m_mutex.locker() == Thread::current().tid());

		auto& header = packet.as<TCPHeader>();
		memset(&header, 0, sizeof(TCPHeader));
		memset(header.options, TCPOption::End, m_tcp_options_bytes);

		header.dst_port = dst_port;
		header.src_port = m_port;
		header.seq_number = m_send_window.current_seq + m_send_window.has_ghost_byte;
		header.ack_number = m_recv_window.start_seq + m_recv_window.data_size + m_recv_window.has_ghost_byte;
		header.data_offset = (sizeof(TCPHeader) + m_tcp_options_bytes) / sizeof(uint32_t);
		header.window_size = BAN::Math::min<size_t>(0xFFFF, m_recv_window.buffer->size() >> m_recv_window.scale_shift);
		header.flags = m_next_flags;
		if (header.flags & FIN)
			m_send_window.has_ghost_byte = true;
		m_next_flags = 0;

		if (m_state == State::Closed || m_state == State::SynReceived)
		{
			add_tcp_header_option<0, TCPOption::MaximumSeqmentSize>(header, m_interface->payload_mtu() - m_network_layer.header_size());

			if (m_connection_info->has_window_scale)
				add_tcp_header_option<4, TCPOption::WindowScale>(header, m_recv_window.scale_shift);
			header.window_size = BAN::Math::min<size_t>(0xFFFF, m_recv_window.buffer->size());

			m_send_window.mss = 1440;
			m_send_window.start_seq++;
			m_send_window.current_seq = m_send_window.start_seq;
		}

		pseudo_header.extra = packet.size();
		header.checksum = calculate_internet_checksum(packet, pseudo_header);

		dprintln_if(DEBUG_TCP, "sending {} {8b}", (uint8_t)m_state, header.flags);
		dprintln_if(DEBUG_TCP, "  {}", (uint32_t)header.ack_number);
		dprintln_if(DEBUG_TCP, "  {}", (uint32_t)header.seq_number);
	}

	void TCPSocket::receive_packet(BAN::ConstByteSpan buffer, const sockaddr* sender, socklen_t sender_len)
	{
		(void)sender_len;

		{
			uint16_t checksum = 0;

			if (sender->sa_family == AF_INET)
			{
				auto& addr_in = *reinterpret_cast<const sockaddr_in*>(sender);
				checksum = calculate_internet_checksum(buffer,
					PseudoHeader {
						.src_ipv4 = BAN::IPv4Address(addr_in.sin_addr.s_addr),
						.dst_ipv4 = m_interface->get_ipv4_address(),
						.protocol = NetworkProtocol::TCP,
						.extra = buffer.size()
					}
				);
			}
			else
			{
				dwarnln("No tcp checksum validation for socket family {}", sender->sa_family);
				return;
			}

			if (checksum != 0)
			{
				dprintln("Checksum does not match");
				return;
			}
		}

		LockGuard _(m_mutex);

		const bool hungup_before = has_hungup_impl();

		auto& header = buffer.as<const TCPHeader>();
		dprintln_if(DEBUG_TCP, "receiving {} {8b}", (uint8_t)m_state, header.flags);
		dprintln_if(DEBUG_TCP, "  {}", (uint32_t)header.ack_number);
		dprintln_if(DEBUG_TCP, "  {}", (uint32_t)header.seq_number);

		m_send_window.non_scaled_size = header.window_size;

		bool check_payload = false;
		switch (m_state)
		{
			case State::Closed:
				break;
			case State::SynSent:
			{
				if (header.flags != (SYN | ACK))
					break;

				if (header.ack_number != m_send_window.current_seq)
				{
					dprintln_if(DEBUG_TCP, "Invalid ack number in SYN/ACK");
					break;
				}

				auto options = parse_tcp_options(header);
				if (options.maximum_seqment_size.has_value())
					m_send_window.mss = *options.maximum_seqment_size;
				if (options.window_scale.has_value())
					m_send_window.scale_shift = *options.window_scale;
				else
				{
					m_recv_window.scale_shift = 0;
					m_connection_info->has_window_scale = false;
				}

				m_send_window.start_seq = m_send_window.current_seq;
				m_send_window.current_ack = m_send_window.current_seq;

				m_recv_window.start_seq = header.seq_number + 1;

				m_next_flags = ACK;
				m_next_state = State::Established;
				break;
			}
			case State::SynReceived:
				if (header.flags != ACK)
					break;
				m_state = State::Established;
				m_has_connected = true;
				break;
			case State::Listen:
				if (header.flags == SYN)
				{
					if (m_pending_connections.size() == m_pending_connections.capacity())
						dprintln_if(DEBUG_TCP, "No storage to store pending connection");
					else
					{
						ConnectionInfo connection_info;
						memcpy(&connection_info.address, sender, sender_len);
						connection_info.address_len = sender_len;
						connection_info.has_window_scale = parse_tcp_options(header).window_scale.has_value();
						MUST(m_pending_connections.emplace(
							connection_info,
							header.seq_number + 1
						));

						epoll_notify(EPOLLIN);
					}
				}
				else
				{
					auto it = m_listen_children.find(ListenKey(sender, sender_len));
					if (it == m_listen_children.end())
					{
						dprintln_if(DEBUG_TCP, "Unexpected packet to listening socket");
						break;
					}
					auto socket = it->value;

					m_mutex.unlock();
					socket->receive_packet(buffer, sender, sender_len);
					m_mutex.lock();

					return;
				}
				break;
			case State::Established:
				check_payload = true;
				if (!(header.flags & FIN))
					break;
				if (m_recv_window.start_seq + m_recv_window.data_size != header.seq_number)
					break;
				m_next_flags = FIN | ACK;
				m_next_state = State::LastAck;
				break;
			case State::CloseWait:
				check_payload = true;
				if (!(header.flags & FIN))
					break;
				m_next_flags = FIN;
				m_next_state = State::LastAck;
				break;
			case State::LastAck:
				check_payload = true;
				if (!(header.flags & ACK))
					break;
				set_connection_as_closed();
				break;
			case State::FinWait1:
				check_payload = true;
				if (!(header.flags & (FIN | ACK)))
					break;
				if ((header.flags & (FIN | ACK)) == (FIN | ACK))
					m_next_state = State::TimeWait;
				else if (header.flags & FIN)
					m_next_state = State::Closing;
				else if (header.flags & ACK)
					m_state = State::FinWait2;
				else
					m_next_flags = ACK;
				break;
			case State::FinWait2:
				check_payload = true;
				if (!(header.flags & FIN))
					break;
				m_next_flags = ACK;
				m_next_state = State::TimeWait;
				break;
			case State::Closing:
				check_payload = true;
				if (!(header.flags & ACK))
					break;
				m_state = State::TimeWait;
				break;
			case State::TimeWait:
				check_payload = true;
				break;
		}

		if (header.seq_number != m_recv_window.start_seq + m_recv_window.data_size + m_recv_window.has_ghost_byte)
			dprintln_if(DEBUG_TCP, "Missing packets");
		else if (check_payload)
		{
			if (header.flags & FIN)
				m_recv_window.has_ghost_byte = true;

			if (header.ack_number > m_send_window.current_ack)
				m_send_window.current_ack = header.ack_number;

			auto payload = buffer.slice(header.data_offset * sizeof(uint32_t));
			if (payload.size() > 0)
			{
				if (m_recv_window.data_size + payload.size() > m_recv_window.buffer->size())
					dprintln_if(DEBUG_TCP, "Cannot fit received bytes to window, waiting for retransmission");
				else
				{
					auto* buffer = reinterpret_cast<uint8_t*>(m_recv_window.buffer->vaddr());
					memcpy(buffer + m_recv_window.data_size, payload.data(), payload.size());
					m_recv_window.data_size += payload.size();

					epoll_notify(EPOLLIN);

					dprintln_if(DEBUG_TCP, "Received {} bytes", payload.size());

					if (m_next_flags == 0)
					{
						m_next_flags = ACK;
						m_next_state = m_state;
					}
				}
			}
		}

		if (!hungup_before && has_hungup_impl())
			epoll_notify(EPOLLHUP);

		m_thread_blocker.unblock();
	}

	void TCPSocket::set_connection_as_closed()
	{
		if (is_bound())
		{
			// NOTE: Only listen socket can unbind the socket as
			//       listen socket is always alive to redirect packets
			if (!m_listen_parent)
				m_network_layer.unbind_socket(m_port);
			else
				m_listen_parent->remove_listen_child(this);
			m_interface = nullptr;
			m_port = PORT_NONE;
			dprintln_if(DEBUG_TCP, "Socket unbound");
		}

		m_thread = nullptr;
	}

	void TCPSocket::remove_listen_child(BAN::RefPtr<TCPSocket> socket)
	{
		LockGuard _(m_mutex);

		auto it = m_listen_children.find(ListenKey(
			reinterpret_cast<const sockaddr*>(&socket->m_connection_info->address),
			socket->m_connection_info->address_len
		));
		if (it == m_listen_children.end())
		{
			dwarnln("remove_listen_child with non-mapped socket");
			return;
		}

		m_listen_children.remove(it);
	}

	void TCPSocket::process_task()
	{
		// FIXME: this should be dynamic
		static constexpr uint32_t retransmit_timeout_ms = 1000;

		BAN::RefPtr<TCPSocket> keep_alive { this };

		// socket's creation did a manual ref(), let's undo it here
		this->unref();

		LockGuard _(m_mutex);

		while (m_thread)
		{
			const uint64_t current_ms = SystemTimer::get().ms_since_boot();

			switch (m_state)
			{
				case State::TimeWait:
					if (current_ms < m_time_wait_start_ms + 30'000)
						break;
					// TimeWait timeout
					set_connection_as_closed();
					continue;
				case State::Listen:
					if (ref_count() > 1)
						break;
					// Listen socket closed
					//    ref_count = keep_alieve
					set_connection_as_closed();
					continue;
				case State::Established:
					if (ref_count() > static_cast<uint32_t>(1 + !!m_listen_parent))
						break;
					// Connected socket closed
					//    ref_count = keep_alive + listen's hashmap
					m_next_flags = FIN | ACK;
					m_next_state = State::FinWait1;
					break;
				default:
					break;
			}

			if (m_next_flags)
			{
				ASSERT(m_connection_info.has_value());
				auto* target_address = reinterpret_cast<const sockaddr*>(&m_connection_info->address);
				auto target_address_len = m_connection_info->address_len;
				if (auto ret = m_network_layer.sendto(*this, {}, target_address, target_address_len); ret.is_error())
					dwarnln("{}", ret.error());
				const bool hungup_before = has_hungup_impl();
				m_state = m_next_state;
				if (m_state == State::Established)
					m_has_connected = true;
				if (!hungup_before && has_hungup_impl())
					epoll_notify(EPOLLHUP);
				continue;
			}

			if (m_send_window.data_size > 0 && m_send_window.current_ack - m_send_window.has_ghost_byte > m_send_window.start_seq)
			{
				uint32_t acknowledged_bytes = m_send_window.current_ack - m_send_window.start_seq - m_send_window.has_ghost_byte;
				ASSERT(acknowledged_bytes <= m_send_window.data_size);

				m_send_window.data_size -= acknowledged_bytes;
				m_send_window.start_seq += acknowledged_bytes;

				if (m_send_window.data_size > 0)
				{
					auto* send_buffer = reinterpret_cast<uint8_t*>(m_send_window.buffer->vaddr());
					memmove(send_buffer, send_buffer + acknowledged_bytes, m_send_window.data_size);
				}

				m_send_window.sent_size -= acknowledged_bytes;

				epoll_notify(EPOLLOUT);

				dprintln_if(DEBUG_TCP, "Target acknowledged {} bytes", acknowledged_bytes);

				continue;
			}

			const bool should_retransmit = m_send_window.data_size > 0 && current_ms >= m_send_window.last_send_ms + retransmit_timeout_ms;

			if (m_send_window.data_size > m_send_window.sent_size || should_retransmit)
			{
				ASSERT(m_connection_info.has_value());
				auto* target_address = reinterpret_cast<const sockaddr*>(&m_connection_info->address);
				auto target_address_len = m_connection_info->address_len;

				const uint32_t send_base = should_retransmit ? 0 : m_send_window.sent_size;

				const uint32_t total_send = BAN::Math::min<uint32_t>(m_send_window.data_size - send_base, m_send_window.scaled_size());

				m_send_window.current_seq = m_send_window.start_seq + m_send_window.sent_size;

				auto* send_buffer = reinterpret_cast<const uint8_t*>(m_send_window.buffer->vaddr() + send_base);
				for (uint32_t i = 0; i < total_send;)
				{
					const uint32_t to_send = BAN::Math::min(total_send - i, m_send_window.mss);

					auto message = BAN::ConstByteSpan(send_buffer + i, to_send);

					m_next_flags = ACK;
					if (auto ret = m_network_layer.sendto(*this, message, target_address, target_address_len); ret.is_error())
					{
						dwarnln("{}", ret.error());
						break;
					}

					dprintln_if(DEBUG_TCP, "Sent {} bytes", to_send);

					m_send_window.sent_size += to_send;
					m_send_window.current_seq += to_send;
					i += to_send;
				}

				m_send_window.last_send_ms = current_ms;

				continue;
			}

			m_thread_blocker.unblock();
			m_thread_blocker.block_with_wake_time_ms(current_ms + retransmit_timeout_ms, &m_mutex);
		}

		m_thread_blocker.unblock();
	}

}
