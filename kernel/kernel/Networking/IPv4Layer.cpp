#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Networking/ICMP.h>
#include <kernel/Networking/IPv4Layer.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/Networking/UDPSocket.h>

#include <netinet/in.h>

#define DEBUG_IPV4 0

namespace Kernel
{

	BAN::ErrorOr<BAN::UniqPtr<IPv4Layer>> IPv4Layer::create()
	{
		auto ipv4_manager = TRY(BAN::UniqPtr<IPv4Layer>::create());
		ipv4_manager->m_process = Process::create_kernel(
			[](void* ipv4_manager_ptr)
			{
				auto& ipv4_manager = *reinterpret_cast<IPv4Layer*>(ipv4_manager_ptr);
				ipv4_manager.packet_handle_task();
			}, ipv4_manager.ptr()
		);
		ASSERT(ipv4_manager->m_process);
		ipv4_manager->m_pending_packet_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			pending_packet_buffer_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true
		));
		ipv4_manager->m_arp_table = TRY(ARPTable::create());
		return ipv4_manager;
	}

	IPv4Layer::IPv4Layer()
	{ }

	IPv4Layer::~IPv4Layer()
	{
		if (m_process)
			m_process->exit(0, SIGKILL);
		m_process = nullptr;
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
		header.checksum			= header.calculate_checksum();
	}

	void IPv4Layer::unbind_socket(uint16_t port, BAN::RefPtr<NetworkSocket> socket)
	{
		LockGuard _(m_lock);
		if (m_bound_sockets.contains(port))
		{
			ASSERT(m_bound_sockets[port].valid());
			ASSERT(m_bound_sockets[port].lock() == socket);
			m_bound_sockets.remove(port);
		}
		NetworkManager::get().TmpFileSystem::remove_from_cache(socket);
	}

	BAN::ErrorOr<void> IPv4Layer::bind_socket(uint16_t port, BAN::RefPtr<NetworkSocket> socket)
	{
		if (NetworkManager::get().interfaces().empty())
			return BAN::Error::from_errno(EADDRNOTAVAIL);

		LockGuard _(m_lock);

		if (port == NetworkSocket::PORT_NONE)
		{
			for (uint32_t temp = 0xC000; temp < 0xFFFF; temp++)
			{
				if (!m_bound_sockets.contains(temp))
				{
					port = temp;
					break;
				}
			}
			if (port == NetworkSocket::PORT_NONE)
			{
				dwarnln("No ports available");
				return BAN::Error::from_errno(EAGAIN);
			}
		}

		if (m_bound_sockets.contains(port))
			return BAN::Error::from_errno(EADDRINUSE);
		TRY(m_bound_sockets.insert(port, socket));

		// FIXME: actually determine proper interface
		auto interface = NetworkManager::get().interfaces().front();
		socket->bind_interface_and_port(interface.ptr(), port);

		return {};
	}

	BAN::ErrorOr<size_t> IPv4Layer::sendto(NetworkSocket& socket, const sys_sendto_t* arguments)
	{
		if (arguments->dest_addr->sa_family != AF_INET)
			return BAN::Error::from_errno(EINVAL);
		auto& sockaddr_in = *reinterpret_cast<const struct sockaddr_in*>(arguments->dest_addr);

		auto dst_port = BAN::host_to_network_endian(sockaddr_in.sin_port);
		auto dst_ipv4 = BAN::IPv4Address { sockaddr_in.sin_addr.s_addr };
		auto dst_mac = TRY(m_arp_table->get_mac_from_ipv4(socket.interface(), dst_ipv4));

		BAN::Vector<uint8_t> packet_buffer;
		TRY(packet_buffer.resize(arguments->length + sizeof(IPv4Header) + socket.protocol_header_size()));
		auto packet = BAN::ByteSpan { packet_buffer.span() };

		memcpy(
			packet.slice(sizeof(IPv4Header)).slice(socket.protocol_header_size()).data(),
			arguments->message,
			arguments->length
		);
		socket.add_protocol_header(
			packet.slice(sizeof(IPv4Header)),
			dst_port
		);
		add_ipv4_header(
			packet,
			socket.interface().get_ipv4_address(),
			dst_ipv4,
			socket.protocol()
		);

		TRY(socket.interface().send_bytes(dst_mac, EtherType::IPv4, packet));

		return arguments->length;
	}

	static uint16_t calculate_internet_checksum(BAN::ConstByteSpan packet)
	{
		uint32_t checksum = 0;
		for (size_t i = 0; i < packet.size() / sizeof(uint16_t); i++)
			checksum += BAN::host_to_network_endian(reinterpret_cast<const uint16_t*>(packet.data())[i]);
		while (checksum >> 16)
			checksum = (checksum >> 16) | (checksum & 0xFFFF);
		return ~(uint16_t)checksum;
	}

	BAN::ErrorOr<void> IPv4Layer::handle_ipv4_packet(NetworkInterface& interface, BAN::ByteSpan packet)
	{
		auto& ipv4_header = packet.as<const IPv4Header>();
		auto ipv4_data = packet.slice(sizeof(IPv4Header));

		ASSERT(ipv4_header.is_valid_checksum());

		auto src_ipv4 = ipv4_header.src_address;
		switch (ipv4_header.protocol)
		{
			case NetworkProtocol::ICMP:
			{
				auto& icmp_header = ipv4_data.as<const ICMPHeader>();
				switch (icmp_header.type)
				{
					case ICMPType::EchoRequest:
					{
						auto dst_mac = TRY(m_arp_table->get_mac_from_ipv4(interface, src_ipv4));

						auto& reply_icmp_header = ipv4_data.as<ICMPHeader>();
						reply_icmp_header.type = ICMPType::EchoReply;
						reply_icmp_header.checksum = 0;
						reply_icmp_header.checksum = calculate_internet_checksum(ipv4_data);

						add_ipv4_header(packet, interface.get_ipv4_address(), src_ipv4, NetworkProtocol::ICMP);

						TRY(interface.send_bytes(dst_mac, EtherType::IPv4, packet));
						break;
					}
					default:
						dprintln("Unhandleded ICMP packet (type {2H})", icmp_header.type);
						break;
				}
				break;
			}
			case NetworkProtocol::UDP:
			{
				auto& udp_header = ipv4_data.as<const UDPHeader>();
				uint16_t src_port = udp_header.src_port;
				uint16_t dst_port = udp_header.dst_port;

				LockGuard _(m_lock);

				if (!m_bound_sockets.contains(dst_port) || !m_bound_sockets[dst_port].valid())
				{
					dprintln_if(DEBUG_IPV4, "no one is listening on port {}", dst_port);
					return {};
				}

				auto udp_data = ipv4_data.slice(sizeof(UDPHeader));
				m_bound_sockets[dst_port].lock()->add_packet(udp_data, src_ipv4, src_port);
				break;
			}
			default:
				dprintln_if(DEBUG_IPV4, "Unknown network protocol 0x{2H}", ipv4_header.protocol);
				break;
		}

		return {};
	}

	void IPv4Layer::packet_handle_task()
	{
		for (;;)
		{
			BAN::Optional<PendingIPv4Packet> pending;

			{
				CriticalScope _;
				if (!m_pending_packets.empty())
				{
					pending = m_pending_packets.front();
					m_pending_packets.pop();
				}
			}

			if (!pending.has_value())
			{
				m_pending_semaphore.block();
				continue;
			}

			uint8_t* buffer_start = reinterpret_cast<uint8_t*>(m_pending_packet_buffer->vaddr());
			const size_t ipv4_packet_size = reinterpret_cast<const IPv4Header*>(buffer_start)->total_length;

			if (auto ret = handle_ipv4_packet(pending->interface, BAN::ByteSpan(buffer_start, ipv4_packet_size)); ret.is_error())
				dwarnln("{}", ret.error());

			CriticalScope _;
			m_pending_total_size -= ipv4_packet_size;
			if (m_pending_total_size)
				memmove(buffer_start, buffer_start + ipv4_packet_size, m_pending_total_size);
		}
	}

	void IPv4Layer::add_ipv4_packet(NetworkInterface& interface, BAN::ConstByteSpan buffer)
	{
		if (m_pending_packets.full())
		{
			dwarnln("IPv4 packet queue full");
			return;
		}

		if (m_pending_total_size + buffer.size() > m_pending_packet_buffer->size())
		{
			dwarnln("IPv4 packet queue full");
			return;
		}

		auto& ipv4_header = buffer.as<const IPv4Header>();
		if (!ipv4_header.is_valid_checksum())
		{
			dwarnln("Invalid IPv4 packet");
			return;
		}
		if (ipv4_header.total_length > buffer.size())
		{
			dwarnln("Too short IPv4 packet");
			return;
		}

		uint8_t* buffer_start = reinterpret_cast<uint8_t*>(m_pending_packet_buffer->vaddr());
		memcpy(buffer_start + m_pending_total_size, buffer.data(), ipv4_header.total_length);
		m_pending_total_size += ipv4_header.total_length;

		m_pending_packets.push({ .interface = interface });
		m_pending_semaphore.unblock();
	}

}
