#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Networking/ICMP.h>
#include <kernel/Networking/IPv4Layer.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/TCPSocket.h>
#include <kernel/Networking/UDPSocket.h>
#include <kernel/Random.h>

#include <netinet/in.h>

namespace Kernel
{

	enum IPv4Flags : uint16_t
	{
		DF = 1 << 14,
	};

	BAN::ErrorOr<BAN::UniqPtr<IPv4Layer>> IPv4Layer::create()
	{
		auto ipv4_manager = TRY(BAN::UniqPtr<IPv4Layer>::create());
		ipv4_manager->m_thread = TRY(Thread::create_kernel(
			[](void* ipv4_manager_ptr)
			{
				auto& ipv4_manager = *reinterpret_cast<IPv4Layer*>(ipv4_manager_ptr);
				ipv4_manager.packet_handle_task();
			}, ipv4_manager.ptr()
		));
		TRY(Processor::scheduler().add_thread(ipv4_manager->m_thread));
		ipv4_manager->m_pending_packet_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			pending_packet_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, false
		));
		ipv4_manager->m_arp_table = TRY(ARPTable::create());
		return ipv4_manager;
	}

	IPv4Layer::IPv4Layer()
	{ }

	IPv4Layer::~IPv4Layer()
	{
		if (m_thread)
			m_thread->add_signal(SIGKILL);
		m_thread = nullptr;
	}

	void IPv4Layer::add_ipv4_header(BAN::ByteSpan packet, BAN::IPv4Address src_ipv4, BAN::IPv4Address dst_ipv4, uint8_t protocol) const
	{
		auto& header = packet.as<IPv4Header>();
		header.version_IHL		= 0x45;
		header.DSCP_ECN			= 0x00;
		header.total_length		= packet.size();
		header.identification	= 1;
		header.flags_frament	= 0x00;
		header.time_to_live		= 0x40;
		header.protocol			= protocol;
		header.src_address		= src_ipv4;
		header.dst_address		= dst_ipv4;
		header.checksum			= 0;
		header.checksum			= calculate_internet_checksum(BAN::ConstByteSpan::from(header), {});
	}

	void IPv4Layer::unbind_socket(uint16_t port)
	{
		SpinLockGuard _(m_bound_socket_lock);
		auto it = m_bound_sockets.find(port);
		ASSERT(it != m_bound_sockets.end());
		m_bound_sockets.remove(it);
	}

	BAN::ErrorOr<void> IPv4Layer::bind_socket_to_unused(BAN::RefPtr<NetworkSocket> socket, const sockaddr* address, socklen_t address_len)
	{
		if (!address || address_len < (socklen_t)sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		if (address->sa_family != AF_INET)
			return BAN::Error::from_errno(EAFNOSUPPORT);
		auto& sockaddr_in = *reinterpret_cast<const struct sockaddr_in*>(address);

		SpinLockGuard _(m_bound_socket_lock);

		uint16_t port = NetworkSocket::PORT_NONE;
		for (uint32_t i = 0; i < 100 && port == NetworkSocket::PORT_NONE; i++)
			if (uint32_t temp = 0xC000 | (Random::get_u32() & 0x3FFF); !m_bound_sockets.contains(temp))
				port = temp;
		for (uint32_t temp = 0xC000; temp < 0xFFFF && port == NetworkSocket::PORT_NONE; temp++)
			if (!m_bound_sockets.contains(temp))
				port = temp;
		if (port == NetworkSocket::PORT_NONE)
		{
			dwarnln("No ports available");
			return BAN::Error::from_errno(EAGAIN);
		}
		dprintln_if(DEBUG_IPV4, "using port {}", port);

		struct sockaddr_in target;
		target.sin_family = AF_INET;
		target.sin_port = BAN::host_to_network_endian(port);
		target.sin_addr.s_addr = sockaddr_in.sin_addr.s_addr;
		return bind_socket_to_address(socket, (sockaddr*)&target, sizeof(sockaddr_in));
	}

	BAN::ErrorOr<void> IPv4Layer::bind_socket_to_address(BAN::RefPtr<NetworkSocket> socket, const sockaddr* address, socklen_t address_len)
	{
		if (!address || address_len < (socklen_t)sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		if (address->sa_family != AF_INET)
			return BAN::Error::from_errno(EAFNOSUPPORT);

		auto& sockaddr_in = *reinterpret_cast<const struct sockaddr_in*>(address);
		const uint16_t port = BAN::host_to_network_endian(sockaddr_in.sin_port);
		if (port == NetworkSocket::PORT_NONE)
			return bind_socket_to_unused(socket, address, address_len);
		const auto ipv4 = BAN::IPv4Address { sockaddr_in.sin_addr.s_addr };

		BAN::RefPtr<NetworkInterface> bind_interface;
		for (auto interface : NetworkManager::get().interfaces())
		{
			if (interface->type() != NetworkInterface::Type::Loopback)
				bind_interface = interface;
			const auto netmask = interface->get_netmask();
			if (ipv4.mask(netmask) != interface->get_ipv4_address().mask(netmask))
				continue;
			bind_interface = interface;
			break;
		}

		if (!bind_interface)
			return BAN::Error::from_errno(EADDRNOTAVAIL);

		SpinLockGuard _(m_bound_socket_lock);

		if (m_bound_sockets.contains(port))
			return BAN::Error::from_errno(EADDRINUSE);
		TRY(m_bound_sockets.insert(port, TRY(socket->get_weak_ptr())));

		socket->bind_interface_and_port(bind_interface.ptr(), port);

		return {};
	}

	BAN::ErrorOr<void> IPv4Layer::get_socket_address(BAN::RefPtr<NetworkSocket> socket, sockaddr* address, socklen_t* address_len)
	{
		if (*address_len < (socklen_t)sizeof(sockaddr_in))
			return BAN::Error::from_errno(ENOBUFS);

		sockaddr_in* in_addr = reinterpret_cast<sockaddr_in*>(address);

		SpinLockGuard _(m_bound_socket_lock);
		for (auto& [bound_port, bound_socket] : m_bound_sockets)
		{
			if (socket != bound_socket.lock())
				continue;
			// FIXME: sockets should have bound address
			in_addr->sin_family = AF_INET;
			in_addr->sin_port = bound_port;
			in_addr->sin_addr.s_addr = INADDR_ANY;
			return {};
		}

		return {};
	}

	BAN::ErrorOr<size_t> IPv4Layer::sendto(NetworkSocket& socket, BAN::ConstByteSpan buffer, const sockaddr* address, socklen_t address_len)
	{
		if (address->sa_family != AF_INET)
			return BAN::Error::from_errno(EINVAL);
		if (address == nullptr || address_len != sizeof(sockaddr_in))
			return BAN::Error::from_errno(EINVAL);
		auto& sockaddr_in = *reinterpret_cast<const struct sockaddr_in*>(address);

		auto dst_port = BAN::host_to_network_endian(sockaddr_in.sin_port);
		auto dst_ipv4 = BAN::IPv4Address { sockaddr_in.sin_addr.s_addr };
		auto dst_mac = TRY(m_arp_table->get_mac_from_ipv4(socket.interface(), dst_ipv4));

		BAN::Vector<uint8_t> packet_buffer;
		TRY(packet_buffer.resize(buffer.size() + sizeof(IPv4Header) + socket.protocol_header_size()));
		auto packet = BAN::ByteSpan { packet_buffer.span() };

		auto pseudo_header = PseudoHeader {
			.src_ipv4 = socket.interface().get_ipv4_address(),
			.dst_ipv4 = dst_ipv4,
			.protocol = socket.protocol()
		};

		memcpy(
			packet.slice(sizeof(IPv4Header)).slice(socket.protocol_header_size()).data(),
			buffer.data(),
			buffer.size()
		);
		socket.add_protocol_header(
			packet.slice(sizeof(IPv4Header)),
			dst_port,
			pseudo_header
		);
		add_ipv4_header(
			packet,
			socket.interface().get_ipv4_address(),
			dst_ipv4,
			socket.protocol()
		);

		TRY(socket.interface().send_bytes(dst_mac, EtherType::IPv4, packet));

		return buffer.size();
	}

	BAN::ErrorOr<void> IPv4Layer::handle_ipv4_packet(NetworkInterface& interface, BAN::ByteSpan packet)
	{
		ASSERT(packet.size() >= sizeof(IPv4Header));
		auto& ipv4_header = packet.as<const IPv4Header>();
		auto ipv4_data = packet.slice(sizeof(IPv4Header));

		auto src_ipv4 = ipv4_header.src_address;

		uint16_t dst_port = NetworkSocket::PORT_NONE;
		uint16_t src_port = NetworkSocket::PORT_NONE;

		switch (ipv4_header.protocol)
		{
			case NetworkProtocol::ICMP:
			{
				if (ipv4_data.size() < sizeof(ICMPHeader))
				{
					dwarnln("IPv4 packet too small for ICMP");
					return {};
				}
				auto& icmp_header = ipv4_data.as<const ICMPHeader>();
				switch (icmp_header.type)
				{
					case ICMPType::EchoRequest:
					{
						auto dst_mac = TRY(m_arp_table->get_mac_from_ipv4(interface, src_ipv4));

						auto& reply_icmp_header = ipv4_data.as<ICMPHeader>();
						reply_icmp_header.type = ICMPType::EchoReply;
						reply_icmp_header.checksum = 0;
						reply_icmp_header.checksum = calculate_internet_checksum(ipv4_data, {});

						add_ipv4_header(packet, interface.get_ipv4_address(), src_ipv4, NetworkProtocol::ICMP);

						TRY(interface.send_bytes(dst_mac, EtherType::IPv4, packet));
						break;
					}
					case ICMPType::DestinationUnreachable:
					{
						auto& ipv4_header = ipv4_data.slice(sizeof(ICMPHeader)).as<const IPv4Header>();
						dprintln("Destination '{}' unreachable, code {2H}", ipv4_header.dst_address, icmp_header.code);
						// FIXME: inform the socket
						break;
					}
					default:
						dprintln("Unhandleded ICMP packet (type {2H})", icmp_header.type);
						break;
				}
				return {};
			}
			case NetworkProtocol::UDP:
			{
				if (ipv4_data.size() < sizeof(UDPHeader))
				{
					dwarnln("IPv4 packet too small for UDP");
					return {};
				}
				auto& udp_header = ipv4_data.as<const UDPHeader>();
				dst_port = udp_header.dst_port;
				src_port = udp_header.src_port;
				break;
			}
			case NetworkProtocol::TCP:
			{
				if (ipv4_data.size() < sizeof(TCPHeader))
				{
					dwarnln("IPv4 packet too small for TCP");
					return {};
				}
				auto& tcp_header = ipv4_data.as<const TCPHeader>();
				dst_port = tcp_header.dst_port;
				src_port = tcp_header.src_port;
				break;
			}
			default:
				dprintln_if(DEBUG_IPV4, "Unknown network protocol 0x{2H}", ipv4_header.protocol);
				return {};
		}

		ASSERT(dst_port != NetworkSocket::PORT_NONE);
		ASSERT(src_port != NetworkSocket::PORT_NONE);

		BAN::RefPtr<Kernel::NetworkSocket> bound_socket;

		{
			SpinLockGuard _(m_bound_socket_lock);
			auto it = m_bound_sockets.find(dst_port);
			if (it == m_bound_sockets.end())
			{
				dprintln_if(DEBUG_IPV4, "no one is listening on port {}", dst_port);
				return {};
			}
			bound_socket = it->value.lock();
		}

		if (!bound_socket)
		{
			dprintln_if(DEBUG_IPV4, "no one is listening on port {}", dst_port);
			return {};
		}

		if (bound_socket->protocol() != ipv4_header.protocol)
		{
			dprintln_if(DEBUG_IPV4, "got data with wrong protocol ({}) on port {} (bound as {})", ipv4_header.protocol, dst_port, (uint8_t)bound_socket->protocol());
			return {};
		}

		sockaddr_in sender;
		sender.sin_family = AF_INET;
		sender.sin_port = BAN::host_to_network_endian(src_port);
		sender.sin_addr.s_addr = src_ipv4.raw;
		bound_socket->receive_packet(ipv4_data, reinterpret_cast<const sockaddr*>(&sender), sizeof(sender));

		return {};
	}

	void IPv4Layer::packet_handle_task()
	{
		for (;;)
		{
			PendingIPv4Packet pending = ({
				SpinLockGuard guard(m_pending_lock);
				while (m_pending_packets.empty())
				{
					SpinLockGuardAsMutex smutex(guard);
					m_pending_thread_blocker.block_indefinite(&smutex);
				}

				auto packet = m_pending_packets.front();
				m_pending_packets.pop();

				packet;
			});

			uint8_t* buffer_start = reinterpret_cast<uint8_t*>(m_pending_packet_buffer->vaddr());
			const size_t ipv4_packet_size = reinterpret_cast<const IPv4Header*>(buffer_start)->total_length;

			if (auto ret = handle_ipv4_packet(pending.interface, BAN::ByteSpan(buffer_start, ipv4_packet_size)); ret.is_error())
				dwarnln_if(DEBUG_IPV4, "{}", ret.error());

			SpinLockGuard _(m_pending_lock);
			m_pending_total_size -= ipv4_packet_size;
			if (m_pending_total_size)
				memmove(buffer_start, buffer_start + ipv4_packet_size, m_pending_total_size);
		}
	}

	void IPv4Layer::add_ipv4_packet(NetworkInterface& interface, BAN::ConstByteSpan buffer)
	{
		if (buffer.size() < sizeof(IPv4Header))
		{
			dwarnln_if(DEBUG_IPV4, "IPv4 packet too small");
			return;
		}

		SpinLockGuard _(m_pending_lock);

		if (m_pending_packets.full())
		{
			dwarnln_if(DEBUG_IPV4, "IPv4 packet queue full");
			return;
		}

		if (m_pending_total_size + buffer.size() > m_pending_packet_buffer->size())
		{
			dwarnln_if(DEBUG_IPV4, "IPv4 packet queue full");
			return;
		}

		auto& ipv4_header = buffer.as<const IPv4Header>();
		if (calculate_internet_checksum(BAN::ConstByteSpan::from(ipv4_header), {}) != 0)
		{
			dwarnln_if(DEBUG_IPV4, "Invalid IPv4 packet");
			return;
		}
		if (ipv4_header.total_length > buffer.size() || ipv4_header.total_length > interface.payload_mtu())
		{
			if (ipv4_header.flags_frament & IPv4Flags::DF)
				dwarnln_if(DEBUG_IPV4, "Invalid IPv4 packet");
			else
				dwarnln_if(DEBUG_IPV4, "IPv4 fragmentation not supported");
			return;
		}

		uint8_t* buffer_start = reinterpret_cast<uint8_t*>(m_pending_packet_buffer->vaddr());
		memcpy(buffer_start + m_pending_total_size, buffer.data(), ipv4_header.total_length);
		m_pending_total_size += ipv4_header.total_length;

		m_pending_packets.push({ .interface = interface });
		m_pending_thread_blocker.unblock();
	}

}
