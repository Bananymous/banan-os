#pragma once

#include <stdint.h>

namespace Kernel
{

	enum E1000_REG : uint32_t
	{
		REG_CTRL	= 0x0000,
		REG_STATUS	= 0x0008,
		REG_EERD	= 0x0014,
		REG_ICR		= 0x00C0,
		REG_ITR		= 0x00C4,
		REG_IMS		= 0x00D0,
		REG_IMC		= 0x00D8,
		REG_IVAR	= 0x00E4,
		REG_EITR	= 0x00E8,
		REG_RCTL	= 0x0100,
		REG_TCTL	= 0x0400,
		REG_TIPG	= 0x0410,
		REG_RDBAL0	= 0x2800,
		REG_RDBAH0	= 0x2804,
		REG_RDLEN0	= 0x2808,
		REG_RDH0	= 0x2810,
		REG_RDT0	= 0x2818,
		REG_TDBAL	= 0x3800,
		REG_TDBAH	= 0x3804,
		REG_TDLEN	= 0x3808,
		REG_TDH		= 0x3810,
		REG_TDT		= 0x3818,
		REG_MTA		= 0x5200,
	};

	enum E1000_CTRL : uint32_t
	{
		CTRL_FD			= 1u << 0,
		CTRL_GIOMD		= 1u << 2,
		CTRL_ASDE		= 1u << 5,
		CTRL_SLU		= 1u << 6,
		CTRL_FRCSPD		= 1u << 11,
		CTRL_FCRDBLX	= 1u << 12,
		CTRL_ADVD3WUC	= 1u << 20,
		CTRL_RST		= 1u << 26,
		CTRL_RFCE		= 1u << 27,
		CTRL_TFCE		= 1u << 28,
		CTRL_VME		= 1u << 30,
		CTRL_PHY_RST	= 1u << 31,

		CTRL_SPEED_10MB		= 0b00 << 8,
		CTRL_SPEED_100MB	= 0b01 << 8,
		CTRL_SPEED_1000MB1	= 0b10 << 8,
		CTRL_SPEED_1000MB2	= 0b11 << 8,
	};

	enum E1000_STATUS : uint32_t
	{
		STATUS_LU = 1 << 1,

		STATUS_SPEED_MASK		= 0b11 << 6,
		STATUS_SPEED_10MB		= 0b00 << 6,
		STATUS_SPEED_100MB		= 0b01 << 6,
		STATUS_SPEED_1000MB1	= 0b10 << 6,
		STATUS_SPEED_1000MB2	= 0b11 << 6,
	};

	enum E1000_ICR : uint32_t
	{
		ICR_TXDW	= 1 << 0,
		ICR_TXQE	= 1 << 1,
		ICR_LSC		= 1 << 2,
		ICR_RXDMT0	= 1 << 4,
		ICR_RXO		= 1 << 6,
		ICR_RXT0	= 1 << 7,
		ICR_MDAC	= 1 << 9,
		ICR_TXD_LOW	= 1 << 15,
		ICR_SRPD	= 1 << 16,
		ICR_ACK		= 1 << 17,
		ICR_MNG		= 1 << 18,
		ICR_RxQ0	= 1 << 20,
		ICR_RxQ1	= 1 << 21,
		ICR_TxQ0	= 1 << 22,
		ICR_TxQ1	= 1 << 23,
		ICR_Other	= 1 << 24,
	};

	enum E1000_IMS : uint32_t
	{
		IMS_TXDW	= 1 << 0,
		IMS_TXQE	= 1 << 1,
		IMS_LSC		= 1 << 2,
		IMS_RXDMT0	= 1 << 4,
		IMS_RXO		= 1 << 6,
		IMS_RXT0	= 1 << 7,
		IMS_MDAC	= 1 << 9,
		IMS_TXD_LOW	= 1 << 15,
		IMS_SRPD	= 1 << 16,
		IMS_ACK		= 1 << 17,
		IMS_MNG		= 1 << 18,
		IMS_RxQ0	= 1 << 20,
		IMS_RxQ1	= 1 << 21,
		IMS_TxQ0	= 1 << 22,
		IMS_TxQ1	= 1 << 23,
		IMS_Other	= 1 << 24,
	};

	enum E1000_IMC : uint32_t
	{
		IMC_TXDW	= 1 << 0,
		IMC_TXQE	= 1 << 1,
		IMC_LSC		= 1 << 2,
		IMC_RXDMT0	= 1 << 4,
		IMC_RXO		= 1 << 6,
		IMC_RXT0	= 1 << 7,
		IMC_MDAC	= 1 << 9,
		IMC_TXD_LOW	= 1 << 15,
		IMC_SRPD	= 1 << 16,
		IMC_ACK		= 1 << 17,
		IMC_MNG		= 1 << 18,
		IMC_RxQ0	= 1 << 20,
		IMC_RxQ1	= 1 << 21,
		IMC_TxQ0	= 1 << 22,
		IMC_TxQ1	= 1 << 23,
		IMC_Other	= 1 << 24,
	};

	enum E1000_TCTL : uint32_t
	{
		TCTL_EN			= 1 << 1,
		TCTL_PSP		= 1 << 3,
		TCTL_CT_IEEE	= 15 << 4,
		TCTL_SWXOFF		= 1 << 22,
		TCTL_PBE		= 1 << 23,
		TCTL_RCTL		= 1 << 24,
		TCTL_UNORTX		= 1 << 25,
		TCTL_MULR		= 1 << 28,
	};

	enum E1000_RCTL : uint32_t
	{
		RCTL_EN		= 1 << 1,
		RCTL_SBP	= 1 << 2,
		RCTL_UPE	= 1 << 3,
		RCTL_MPE	= 1 << 4,
		RCTL_BAM	= 1 << 15,
		RCTL_VFE	= 1 << 18,
		RCTL_CFIEN	= 1 << 19,
		RCTL_CFI	= 1 << 20,
		RCTL_DPF	= 1 << 22,
		RCTL_PMCF	= 1 << 23,
		RCTL_BSEX	= 1 << 25,
		RCTL_SECRC	= 1 << 26,

		RCTL_RDMTS_1_2	= 0b00 << 8,
		RCTL_RDMTS_1_4	= 0b01 << 8,
		RCTL_RDMTS_1_8	= 0b10 << 8,

		RCTL_LBM_NORMAL	= 0b00 << 6,
		RCTL_LBM_MAC	= 0b01 << 6,

		RCTL_BSIZE_256		= (0b11 << 16),
		RCTL_BSIZE_512		= (0b10 << 16),
		RCTL_BSIZE_1024		= (0b01 << 16),
		RCTL_BSIZE_2048		= (0b00 << 16),
		RCTL_BSIZE_4096		= (0b11 << 16) | RCTL_BSEX,
		RCTL_BSIZE_8192		= (0b10 << 16) | RCTL_BSEX,
		RCTL_BSIZE_16384	= (0b01 << 16) | RCTL_BSEX,
	};

	enum E1000_CMD : uint8_t
	{
		CMD_EOP		= 1 << 0,
		CMD_IFCS	= 1 << 1,
		CMD_IC		= 1 << 2,
		CMD_RS		= 1 << 3,
		CMD_DEXT	= 1 << 5,
		CMD_VLE		= 1 << 6,
		CMD_IDE		= 1 << 7,
	};

	struct e1000_rx_desc
	{
		uint64_t addr;
		uint16_t length;
		uint16_t checksum;
		uint8_t status;
		uint8_t errors;
		uint16_t special;
	} __attribute__((packed));

	struct e1000_tx_desc
	{
		uint64_t addr;
		uint16_t length;
		uint8_t cso;
		uint8_t cmd;
		uint8_t status;
		uint8_t css;
		uint16_t special;
	} __attribute__((packed));

}
