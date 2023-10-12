#pragma once

#include <stdint.h>

#define FIS_TYPE_REGISTER_H2D		0x27
#define FIS_TYPE_REGISTER_D2H		0x34
#define FIS_TYPE_DMA_ACT			0x39
#define FIS_TYPE_DMA_SETUP			0x41
#define FIS_TYPE_DATA				0x46
#define FIS_TYPE_BIST				0x58
#define FIS_TYPE_PIO_SETUP			0x5F
#define FIS_TYPE_SET_DEVIVE_BITS	0xA1

#define SATA_CAP_SUPPORTS64	(1 << 31)

#define SATA_GHC_AHCI_ENABLE		(1 << 31)
#define SATA_GHC_INTERRUPT_ENABLE	(1 << 1)

#define	SATA_SIG_ATA	0x00000101
#define	SATA_SIG_ATAPI	0xEB140101
#define	SATA_SIG_SEMB	0xC33C0101
#define	SATA_SIG_PM		0x96690101

#define HBA_PORT_IPM_ACTIVE		1
#define HBA_PORT_DET_PRESENT	3

#define HBA_PxCMD_ST	0x0001
#define HBA_PxCMD_FRE	0x0010
#define HBA_PxCMD_FR	0x4000
#define HBA_PxCMD_CR	0x8000

namespace Kernel
{

	static constexpr uint32_t s_hba_prdt_count { 8 };

	struct FISRegisterH2D
	{
		uint8_t  fis_type;			// FIS_TYPE_REGISTER_H2D
	
		uint8_t  pm_port : 4;		// Port multiplier
		uint8_t  __reserved0 : 3;
		uint8_t  c : 1;				// 1: Command, 0: Control
	
		uint8_t  command;
		uint8_t  feature_lo;		// Feature register, 7:0
	
		uint8_t  lba0;				// LBA low register, 7:0
		uint8_t  lba1;				// LBA mid register, 15:8
		uint8_t  lba2;				// LBA high register, 23:16
		uint8_t  device;
	
		uint8_t  lba3;				// LBA register, 31:24
		uint8_t  lba4;				// LBA register, 39:32
		uint8_t  lba5;				// LBA register, 47:40
		uint8_t  feature_hi;		// Feature register, 15:8
	
		uint8_t  count_lo;			// Count register, 7:0
		uint8_t  count_hi;			// Count register, 15:8
		uint8_t  icc;				// Isochronous command completion
		uint8_t  control;
	
		uint8_t  __reserved1[4];
	} __attribute__((packed));

	struct FISRegisterD2H
	{
		uint8_t  fis_type;			// FIS_TYPE_REGISTER_D2H
	
		uint8_t  pm_port : 4;		// Port multiplier
		uint8_t  __reserved0 : 2;
		uint8_t  i : 1;				// Interrupt bit
		uint8_t  __reserved1 : 1;
	
		uint8_t  status;
		uint8_t  error;
	
		uint8_t  lba0;				// LBA low register, 7:0
		uint8_t  lba1;				// LBA mid register, 15:8
		uint8_t  lba2;				// LBA high register, 23:16
		uint8_t  device;
	
		uint8_t  lba3;				// LBA register, 31:24
		uint8_t  lba4;				// LBA register, 39:32
		uint8_t  lba5;				// LBA register, 47:40
		uint8_t  __reserved2;
	
		uint8_t  count_lo;			// Count register, 7:0
		uint8_t  count_hi;			// Count register, 15:8
		uint8_t  __reserved3[2];
	
		uint8_t  __reserved4[4];
	} __attribute__((packed));

	struct FISDataBI
	{
		uint8_t  fis_type;			// FIS_TYPE_DATA
	
		uint8_t  pm_port : 4;		// Port multiplier
		uint8_t  __reserved0 : 4;
	
		uint8_t  __reserved1[2];

		uint32_t data[0];			// Payload (1 - 2048 dwords)
	} __attribute__((packed));

	struct SetDeviceBitsD2H
	{
		uint8_t fis_type;		// FIS_TYPE_SET_DEVICE_BITS

		uint8_t pm_port : 4;	// Port multiplier
		uint8_t __reserved0 : 2;
		uint8_t i : 1;			// Interrupt bit
		uint8_t n : 1;			// Notification bit

		uint8_t status;
		uint8_t error;

		uint32_t __reserved1;
	} __attribute__((packed));

	struct PIOSetupD2H
	{
		uint8_t  fis_type;			// FIS_TYPE_PIO_SETUP
	
		uint8_t  pm_port : 4;		// Port multiplier
		uint8_t  __reserved0 : 1;
		uint8_t  d : 1;				// Data transfer direction, 1 - device to host
		uint8_t  i : 1;				// Interrupt bit
		uint8_t  __reserved1 : 1;
	
		uint8_t  status;
		uint8_t  error;
	
		uint8_t  lba0;				// LBA low register, 7:0
		uint8_t  lba1;				// LBA mid register, 15:8
		uint8_t  lba2;				// LBA high register, 23:16
		uint8_t  device;
	
		uint8_t  lba3;				// LBA register, 31:24
		uint8_t  lba4;				// LBA register, 39:32
		uint8_t  lba5;				// LBA register, 47:40
		uint8_t  __reserved2;
	
		uint8_t  count_lo;			// Count register, 7:0
		uint8_t  count_hi;			// Count register, 15:8
		uint8_t  __reserved3;
		uint8_t  e_status;			// New value of status register
	
		uint16_t tc;				// Transfer count
		uint8_t  __reserved4[2];
	} __attribute__((packed));

	struct DMASetupBI
	{
		uint8_t  fis_type;				// FIS_TYPE_DMA_SETUP
	
		uint8_t  pm_port : 4;			// Port multiplier
		uint8_t  __reserved0 : 1;
		uint8_t  d : 1;					// Data transfer direction, 1 - device to host
		uint8_t  i : 1;					// Interrupt bit
		uint8_t  a : 1;					// Auto-activate. Specifies if DMA Activate FIS is needed
	
		uint8_t  __reserved1[2];

		uint64_t dma_buffer_id;			// DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
										// SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

		uint32_t __reserved2;

		uint32_t dma_buffer_offset;		// Byte offset into buffer. First 2 bits must be 0

		uint32_t dma_transfer_count;	// Number of bytes to transfer. Bit 0 must be 0

		uint32_t __reserved3;
	} __attribute__((packed));

	struct HBAPortMemorySpace
	{
		uint32_t clb;		// command list base address, 1K-byte aligned
		uint32_t clbu;		// command list base address upper 32 bits
		uint32_t fb;		// FIS base address, 256-byte aligned
		uint32_t fbu;		// FIS base address upper 32 bits
		uint32_t is;		// interrupt status
		uint32_t ie;		// interrupt enable
		uint32_t cmd;		// command and status
		uint32_t __reserved0;
		uint32_t tfd;		// task file data
		uint32_t sig;		// signature
		uint32_t ssts;		// SATA status (SCR0:SStatus)
		uint32_t sctl;		// SATA control (SCR2:SControl)
		uint32_t serr;		// SATA error (SCR1:SError)
		uint32_t sact;		// SATA active (SCR3:SActive)
		uint32_t ci;		// command issue
		uint32_t sntf;		// SATA notification (SCR4:SNotification)
		uint32_t fbs;		// FIS-based switch control
		uint32_t __reserved1[11];
		uint32_t vendor[4];
	} __attribute__((packed));

	struct HBAGeneralMemorySpace
	{
		uint32_t cap;		// Host capability
		uint32_t ghc;		// Global host control
		uint32_t is;		// Interrupt status
		uint32_t pi;		// Port implemented
		uint32_t vs;		// Version
		uint32_t ccc_ctl;	// Command completion coalescing control
		uint32_t ccc_pts;	// Command completion coalescing ports
		uint32_t em_loc;	// 0x1C, Enclosure management location
		uint32_t em_ctl;	// 0x20, Enclosure management control
		uint32_t cap2;		// 0x24, Host capabilities extended
		uint32_t bohc;		// 0x28, BIOS/OS handoff control and status
	
		uint8_t  __reserved0[0xA0-0x2C];
	
		uint8_t  vendor[0x100-0xA0];
	
		HBAPortMemorySpace ports[0]; // 1 - 32 ports
	} __attribute__((packed));

	struct ReceivedFIS
	{
		DMASetupBI			dsfis;
		uint8_t				pad0[4];
	
		PIOSetupD2H			psfis;
		uint8_t				pad1[12];
	
		FISRegisterD2H		rfis;
		uint8_t				pad2[4];
	
		SetDeviceBitsD2H	sdbfis;
	
		uint8_t				ufis[64];
	
		uint8_t				__reserved[0x100-0xA0];
	} __attribute__((packed));

	struct HBACommandHeader
	{
		uint8_t  cfl : 5;	// Command FIS length in DWORDS, 2 ~ 16
		uint8_t  a : 1;		// ATAPI
		uint8_t  w : 1;		// Write, 1: H2D, 0: D2H
		uint8_t  p : 1;		// Prefetchable
	
		uint8_t  r : 1;		// Reset
		uint8_t  b : 1;		// BIST
		uint8_t  c : 1;		// Clear busy upon R_OK
		uint8_t  __reserved0 : 1;
		uint8_t  pmp : 4;	// Port multiplier port
	
		uint16_t prdtl;		// Physical region descriptor table length in entries
	
		volatile uint32_t prdbc;	// Physical region descriptor byte count transferred
	
		uint32_t ctba;		// Command table descriptor base address
		uint32_t ctbau;		// Command table descriptor base address upper 32 bits
	
		uint32_t __reserved1[4];
	} __attribute__((packed));

	struct HBAPRDTEntry
	{
		uint32_t dba;			// Data base address
		uint32_t dbau;			// Data base address upper 32 bits
		uint32_t __reserved0;

		uint32_t dbc : 22;		// Byte count, 4M max
		uint32_t __reserved1 : 9;
		uint32_t i : 1;			// Interrupt on completion
	} __attribute__((packed));

	struct HBACommandTable
	{
		uint8_t cfis[64];
		uint8_t acmd[16];
		uint8_t __reserved[48];
		HBAPRDTEntry prdt_entry[s_hba_prdt_count];
	} __attribute__((packed));

	enum class AHCIPortType
	{
		NONE,
		SATA,
		SATAPI,
		SEMB,
		PM
	};

	class AHCIController;
	class AHCIDevice;

}
