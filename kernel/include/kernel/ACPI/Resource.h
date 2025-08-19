#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Debug.h>
#include <BAN/Optional.h>

namespace Kernel::ACPI
{

	struct ResourceData
	{
		enum class Type
		{
			IRQ,
			DMA,
			IOPort,
			FixedIOPort,
			FixedDMA,

			// TODO: large stuff the stuff :)
		};

		union {
			struct {
				uint16_t irq_mask;
				union {
					struct {
						uint8_t edge_triggered : 1;
						uint8_t                : 2;
						uint8_t active_low     : 1;
						uint8_t shared         : 1;
						uint8_t wake_capable   : 1;
						uint8_t                : 2;
					};
					uint8_t raw;
				};
			} irq;
			struct {
				uint8_t channel_mask;
				union {
					struct {
						uint8_t type          : 2; // 0: 8 bit, 1: 8 and 16 bit, 2: 16 bit only
						uint8_t bus_master    : 1;
						uint8_t               : 2;
						uint8_t channel_speed : 2; // 0: compatibility, 1: type A, 2: type B, 3: type F
						uint8_t               : 1;
					};
					uint8_t raw;
				};
			} dma;
			struct {
				uint16_t range_min_base;
				uint16_t range_max_base;
				uint8_t base_alignment;
				uint8_t range_length;
			} io_port;
			struct {
				uint16_t range_base;
				uint8_t range_length;
			} fixed_io_port;
			struct {
				uint16_t request_line;
				uint16_t channel;
				uint8_t transfer_width; // 0: 8 bit, 1: 16 bit, 2: 32 bit, 3: 64 bit, 4: 128 bit
			} fixed_dma;
		} as;
		Type type;
	};

	class ResourceParser
	{
	public:
		ResourceParser(BAN::ConstByteSpan buffer)
			: m_buffer(buffer)
		{}

		BAN::Optional<ResourceData> get_next()
		{
			for (;;)
			{
				if (m_buffer.empty())
					return {};

				if (m_buffer[0] & 0x80)
				{
					dprintln("Skipping large resource 0x{2H}", m_buffer[0] & 0x7F);
					const uint16_t length = (m_buffer[2] << 8) | m_buffer[1];
					if (m_buffer.size() < static_cast<size_t>(3 + length))
						return {};
					m_buffer = m_buffer.slice(3 + length);
					continue;
				}

				const uint8_t length = m_buffer[0] & 0x07;
				if (m_buffer.size() < static_cast<size_t>(1 + length))
					return {};

				BAN::Optional<ResourceData> result;
				switch ((m_buffer[0] >> 3) & 0x0F)
				{
					case 0x04:
						if (length < 2)
							break;
						result = ResourceData {
							.as = { .irq = {
								.irq_mask = static_cast<uint16_t>((m_buffer[2] << 8) | m_buffer[1]),
								.raw = (length >= 3) ? m_buffer[3] : static_cast<uint8_t>(1),
							}},
							.type = ResourceData::Type::IRQ,
						};
						break;
					case 0x05:
						if (length < 2)
							break;
						result = ResourceData {
							.as = { .dma = {
								.channel_mask = m_buffer[1],
								.raw = m_buffer[2],
							}},
							.type = ResourceData::Type::DMA,
						};
						break;
					case 0x08:
						if (length < 7)
							break;
						result = ResourceData {
							.as = { .io_port = {
								.range_min_base = static_cast<uint16_t>(((m_buffer[3] << 8) | m_buffer[2]) & ((m_buffer[1] & 1) ? 0xFFFF : 0x03FF)),
								.range_max_base = static_cast<uint16_t>(((m_buffer[5] << 8) | m_buffer[4]) & ((m_buffer[1] & 1) ? 0xFFFF : 0x03FF)),
								.base_alignment = m_buffer[6],
								.range_length = m_buffer[7],
							}},
							.type = ResourceData::Type::IOPort,
						};
						break;
					case 0x09:
						if (length < 3)
							break;
						result = ResourceData {
							.as = { .fixed_io_port = {
								.range_base = static_cast<uint16_t>(((m_buffer[2] << 8) | m_buffer[1]) & 0x03FF),
								.range_length = m_buffer[3],
							}},
							.type = ResourceData::Type::FixedIOPort,
						};
						break;
					case 0x0A:
						if (length < 5)
							break;
						result = ResourceData {
							.as = { .fixed_dma = {
								.request_line = static_cast<uint16_t>((m_buffer[2] << 8) | m_buffer[1]),
								.channel      = static_cast<uint16_t>((m_buffer[4] << 8) | m_buffer[3]),
								.transfer_width = m_buffer[5],
							}},
							.type = ResourceData::Type::FixedDMA,
						};
						break;
					case 0x0F:
						// End tag
						return {};
					case 0x06:
					case 0x07:
					case 0x0E:
						dprintln("Skipping short resource 0x{2H}", (m_buffer[0] >> 3) & 0x0F);
						break;
				}

				m_buffer = m_buffer.slice(1 + length);
				if (result.has_value())
					return result.release_value();
			}
		}

	private:
		BAN::ConstByteSpan m_buffer;
	};

}
