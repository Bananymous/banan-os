#pragma once

#include <stdint.h>

namespace Kernel
{

	enum RTL8169_IO_REGS : uint16_t
	{
		RTL8169_IO_IDR0   = 0x00,
		RTL8169_IO_IDR1   = 0x01,
		RTL8169_IO_IDR2   = 0x02,
		RTL8169_IO_IDR3   = 0x03,
		RTL8169_IO_IDR4   = 0x04,
		RTL8169_IO_IDR5   = 0x05,

		RTL8169_IO_TNPDS  = 0x20,
		RTL8169_IO_CR     = 0x37,
		RTL8169_IO_TPPoll = 0x38,
		RTL8169_IO_IMR    = 0x3C,
		RTL8169_IO_ISR    = 0x3E,
		RTL8169_IO_TCR    = 0x40,
		RTL8169_IO_RCR    = 0x44,
		RTL8169_IO_9346CR = 0x50,
		RTL8169_IO_PHYSts = 0x6C,
		RTL8169_IO_RMS    = 0xDA,
		RTL8169_IO_RDSAR  = 0xE4,
		RTL8169_IO_MTPS   = 0xEC,
	};

	enum RTL8169_CR : uint8_t
	{
		RTL8169_CR_TE  = 1u << 2,
		RTL8169_CR_RE  = 1u << 3,
		RTL8169_CR_RST = 1u << 4,
	};

	enum RTL8169_TPPoll : uint8_t
	{
		RTL8169_TPPoll_FSWInt = 1u << 0,
		RTL8169_TPPoll_NPQ    = 1u << 6,
		RTL8169_TPPoll_HPQ    = 1u << 7,
	};

	enum RTL8169_IR : uint16_t
	{
		RTL8169_IR_ROK     = 1u << 0,
		RTL8169_IR_RER     = 1u << 1,
		RTL8169_IR_TOK     = 1u << 2,
		RTL8169_IR_TER     = 1u << 3,
		RTL8169_IR_RDU     = 1u << 4,
		RTL8169_IR_LinkChg = 1u << 5,
		RTL8169_IR_FVOW    = 1u << 6,
		RTL8169_IR_TDU     = 1u << 7,
		RTL8169_IR_SWInt   = 1u << 8,
		RTL8169_IR_FEmp    = 1u << 9,
		RTL8169_IR_TimeOut = 1u << 14,
	};

	enum RTL8169_TCR : uint32_t
	{
		RTL8169_TCR_MXDMA_16        = 0b000u << 8,
		RTL8169_TCR_MXDMA_32        = 0b001u << 8,
		RTL8169_TCR_MXDMA_64        = 0b010u << 8,
		RTL8169_TCR_MXDMA_128       = 0b011u << 8,
		RTL8169_TCR_MXDMA_256       = 0b100u << 8,
		RTL8169_TCR_MXDMA_512       = 0b101u << 8,
		RTL8169_TCR_MXDMA_1024      = 0b110u << 8,
		RTL8169_TCR_MXDMA_UNLIMITED = 0b111u << 8,

		RTL8169_TCR_TX_NOCRC        = 1u << 16,

		RTL8169_TCR_LOOPBACK        = 0b01u << 17,

		RTL8169_TCR_IFG_0           = (0u << 19) | (0b11 << 24),
		RTL8169_TCR_IFG_8           = (1u << 19) | (0b01 << 24),
		RTL8169_TCR_IFG_16          = (1u << 19) | (0b11 << 24),
		RTL8169_TCR_IFG_24          = (0u << 19) | (0b01 << 24),
		RTL8169_TCR_IFG_48          = (0u << 19) | (0b10 << 24),
	};

	enum RTL8169_RCR : uint32_t
	{
		RTL8169_RCR_AAP             = 1u << 0, // accept all packets with destination
		RTL8169_RCR_APM             = 1u << 1, // accept physical matches
		RTL8169_RCR_AM              = 1u << 2, // accept multicast
		RTL8169_RCR_AB              = 1u << 3, // accept broadcast
		RTL8169_RCR_AR              = 1u << 4, // accept runt
		RTL8169_RCR_AER             = 1u << 5, // accept error

		RTL8169_RCR_MXDMA_64        = 0b010u << 8,
		RTL8169_RCR_MXDMA_128       = 0b011u << 8,
		RTL8169_RCR_MXDMA_256       = 0b100u << 8,
		RTL8169_RCR_MXDMA_512       = 0b101u << 8,
		RTL8169_RCR_MXDMA_1024      = 0b110u << 8,
		RTL8169_RCR_MXDMA_UNLIMITED = 0b111u << 8,

		RTL8169_RCR_RXFTH_64        = 0b010u << 13,
		RTL8169_RCR_RXFTH_128       = 0b011u << 13,
		RTL8169_RCR_RXFTH_256       = 0b100u << 13,
		RTL8169_RCR_RXFTH_512       = 0b101u << 13,
		RTL8169_RCR_RXFTH_1024      = 0b110u << 13,
		RTL8169_RCR_RXFTH_NO        = 0b111u << 13,
	};

	enum RTL8169_9346CR : uint8_t
	{
		RTL8169_9346CR_MODE_NORMAL = 0b00 << 6,
		RTL8169_9346CR_MODE_CONFIG = 0b11 << 6,
	};

	enum RTL8169_PHYSts : uint8_t
	{
		RTL8169_PHYSts_FullDup = 1u << 0,
		RTL8169_PHYSts_LinkSts = 1u << 1,
		RTL8169_PHYSts_10M     = 1u << 2,
		RTL8169_PHYSts_100M    = 1u << 3,
		RTL8169_PHYSts_1000MF  = 1u << 4,
		RTL8169_PHYSts_RxFlow  = 1u << 5,
		RTL8169_PHYSts_TxFlow  = 1u << 6,
	};

	enum RTL8169_RMS : uint16_t
	{
		// 8192 - 1
		RTL8169_RMS_MAX = 0x1FFF,
	};

	enum RTL8169_MTPS : uint8_t
	{
		RTL8169_MTPS_MAX = 0x3B,
	};

	struct RTL8169Descriptor
	{
		uint32_t command;
		uint32_t vlan;
		uint32_t buffer_low;
		uint32_t buffer_high;
	};

	enum RTL8169DescriptorCommand : uint32_t
	{
		RTL8169_DESC_CMD_LGSEN = 1u << 27,
		RTL8169_DESC_CMD_LS    = 1u << 28,
		RTL8169_DESC_CMD_FS    = 1u << 29,
		RTL8169_DESC_CMD_EOR   = 1u << 30,
		RTL8169_DESC_CMD_OWN   = 1u << 31,
	};

}
