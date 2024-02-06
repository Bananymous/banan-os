#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/CircularQueue.h>
#include <BAN/Endianness.h>
#include <BAN/IPv4.h>
#include <BAN/NoCopyMove.h>
#include <BAN/UniqPtr.h>
#include <kernel/Networking/ARPTable.h>
#include <kernel/Networking/NetworkInterface.h>
#include <kernel/Networking/NetworkLayer.h>
#include <kernel/Networking/NetworkSocket.h>
#include <kernel/Process.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	struct IPv4Header
	{
		uint8_t version_IHL;
		uint8_t DSCP_ECN;
		BAN::NetworkEndian<uint16_t> total_length	{ 0 };
		BAN::NetworkEndian<uint16_t> identification	{ 0 };
		BAN::NetworkEndian<uint16_t> flags_frament	{ 0 };
		uint8_t time_to_live;
		uint8_t protocol;
		BAN::NetworkEndian<uint16_t> checksum		{ 0 };
		BAN::IPv4Address src_address;
		BAN::IPv4Address dst_address;

		constexpr uint16_t calculate_checksum() const
		{
			uint32_t total_sum = 0;
			for (size_t i = 0; i < sizeof(IPv4Header) / sizeof(uint16_t); i++)
				total_sum += reinterpret_cast<const BAN::NetworkEndian<uint16_t>*>(this)[i];
			total_sum -= checksum;
			while (total_sum >> 16)
				total_sum = (total_sum >> 16) + (total_sum & 0xFFFF);
			return ~(uint16_t)total_sum;
		}

		constexpr bool is_valid_checksum() const
		{
			uint32_t total_sum = 0;
			for (size_t i = 0; i < sizeof(IPv4Header) / sizeof(uint16_t); i++)
				total_sum += reinterpret_cast<const BAN::NetworkEndian<uint16_t>*>(this)[i];
			while (total_sum >> 16)
				total_sum = (total_sum >> 16) + (total_sum & 0xFFFF);
			return total_sum == 0xFFFF;
		}
	};
	static_assert(sizeof(IPv4Header) == 20);

	class IPv4Layer : public NetworkLayer
	{
		BAN_NON_COPYABLE(IPv4Layer);
		BAN_NON_MOVABLE(IPv4Layer);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<IPv4Layer>> create();
		~IPv4Layer();

		ARPTable& arp_table() { return *m_arp_table; }

		void add_ipv4_packet(NetworkInterface&, BAN::ConstByteSpan);

		virtual void unbind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) override;
		virtual BAN::ErrorOr<void> bind_socket(uint16_t port, BAN::RefPtr<NetworkSocket>) override;

		virtual BAN::ErrorOr<size_t> sendto(NetworkSocket&, const sys_sendto_t*) override;

	private:
		IPv4Layer();

		void add_ipv4_header(BAN::ByteSpan packet, BAN::IPv4Address src_ipv4, BAN::IPv4Address dst_ipv4, uint8_t protocol) const;

		void packet_handle_task();
		BAN::ErrorOr<void> handle_ipv4_packet(NetworkInterface&, BAN::ByteSpan);

	private:
		struct PendingIPv4Packet
		{
			NetworkInterface& interface;
		};

	private:
		SpinLock				m_lock;

		BAN::UniqPtr<ARPTable>	m_arp_table;
		Process*				m_process { nullptr };

		static constexpr size_t pending_packet_buffer_size = 128 * PAGE_SIZE;
		BAN::UniqPtr<VirtualRange>					m_pending_packet_buffer;
		BAN::CircularQueue<PendingIPv4Packet, 128>	m_pending_packets;
		Semaphore									m_pending_semaphore;
		size_t										m_pending_total_size { 0 };

		BAN::HashMap<int, BAN::WeakPtr<NetworkSocket>>	m_bound_sockets;

		friend class BAN::UniqPtr<IPv4Layer>;
	};

}
