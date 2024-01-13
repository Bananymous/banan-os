#pragma once

#include <stdint.h>

namespace Kernel::NVMe
{

	struct CAP
	{
		uint64_t mqes : 16;
		uint64_t cqr : 1;
		uint64_t ams : 2;
		uint64_t __reserved0 : 5;
		uint64_t to : 8;
		uint64_t dstrd : 4;
		uint64_t nssrs : 1;
		uint64_t css : 8;
		uint64_t bps : 1;
		uint64_t cps : 2;
		uint64_t mpsmin : 4;
		uint64_t mpsmax : 4;
		uint64_t pmrs : 1;
		uint64_t cmpbs : 1;
		uint64_t nsss : 1;
		uint64_t crms : 2;
		uint64_t __reserved1 : 3;
	};
	static_assert(sizeof(CAP) == sizeof(uint64_t));

	enum CAP_CSS
	{
		CAP_CSS_NVME  = 1 << 0,
		CAP_CSS_IO    = 1 << 6,
		CAP_CSS_ADMIN = 1 << 7,
	};

	struct VS
	{
		uint32_t tertiary : 8;
		uint32_t minor : 8;
		uint32_t major : 16;
	};
	static_assert(sizeof(VS) == sizeof(uint32_t));

	struct CC
	{
		uint32_t en : 1;
		uint32_t __reserved0 : 3;
		uint32_t css : 3;
		uint32_t mps : 4;
		uint32_t ams : 3;
		uint32_t shn : 2;
		uint32_t iosqes : 4;
		uint32_t iocqes : 4;
		uint32_t crime : 1;
		uint32_t __reserved1 : 7;
	};
	static_assert(sizeof(CC) == sizeof(uint32_t));

	struct CSTS
	{
		uint32_t rdy : 1;
		uint32_t cfs : 1;
		uint32_t shts : 2;
		uint32_t nssro : 1;
		uint32_t pp : 1;
		uint32_t st : 1;
		uint32_t __reserved : 25;
	};
	static_assert(sizeof(CSTS) == sizeof(uint32_t));

	struct AQA
	{
		uint32_t asqs : 12;
		uint32_t __reserved0 : 4;
		uint32_t acqs : 12;
		uint32_t __reserved1 : 4;
	};
	static_assert(sizeof(AQA) == sizeof(uint32_t));

	// BAR0
	struct ControllerRegisters
	{
		CAP cap;
		VS vs;
		uint32_t intms;
		uint32_t intmc;
		CC cc;
		uint8_t __reserved0[4];
		CSTS csts;
		uint32_t nssr;
		AQA aqa;
		uint64_t asq;
		uint64_t acq;

		static constexpr uint32_t SQ0TDBL = 0x1000;
	};
	static_assert(sizeof(ControllerRegisters) == 0x38);

	struct DoorbellRegisters
	{
		uint32_t sq_tail;
		uint32_t cq_head;
	} __attribute__((packed));

	struct CompletionQueueEntry
	{
		uint32_t dontcare[3];
		uint16_t cid;
		uint16_t sts;
	} __attribute__((packed));
	static_assert(sizeof(CompletionQueueEntry) == 16);

	struct DataPtr
	{
		union
		{
			struct
			{
				uint64_t prp1;
				uint64_t prp2;
			};
			uint8_t sgl1[16];
		};
	};

	struct CommandGeneric
	{
		uint32_t nsid;
		uint32_t cdw2;
		uint32_t cdw3;
		uint64_t mptr;
		DataPtr dptr;
		uint32_t cdw10;
		uint32_t cdw11;
		uint32_t cdw12;
		uint32_t cdw13;
		uint32_t cdw14;
		uint32_t cdw15;
	} __attribute__((packed));
	static_assert(sizeof(CommandGeneric) == 15 * sizeof(uint32_t));

	struct CommandIdentify
	{
		uint32_t nsid;
		uint64_t __reserved0[2];
		DataPtr dptr;
		// dword 10
		uint8_t cns;
		uint8_t __reserved1;
		uint16_t cntid;
		// dword 11 
		uint16_t cnsid;
		uint8_t __reserved2;
		uint8_t csi;
		// dword 12-15
		uint32_t __reserved3[4];
	} __attribute__((packed));
	static_assert(sizeof(CommandIdentify) == 15 * sizeof(uint32_t));

	struct CommandCreateCQ
	{
		uint32_t __reserved0;
		uint64_t __reserved1[2];
		DataPtr dptr;
		// dword 10
		uint16_t qid;
		uint16_t qsize;
		// dword 11
		uint16_t pc : 1;
		uint16_t ien : 1;
		uint16_t __reserved2 : 14;
		uint16_t iv;
		// dword 12-15
		uint32_t __reserved4[4];
	} __attribute__((packed));
	static_assert(sizeof(CommandCreateCQ) == 15 * sizeof(uint32_t));

	struct CommandCreateSQ
	{
		uint32_t __reserved0;
		uint64_t __reserved1[2];
		DataPtr dptr;
		// dword 10
		uint16_t qid;
		uint16_t qsize;
		// dword 11
		uint16_t pc : 1;
		uint16_t qprio : 2;
		uint16_t __reserved2 : 13;
		uint16_t cqid;
		// dword 12
		uint16_t nvmsetid;
		uint16_t __reserved4;
		// dword 13-15
		uint32_t __reserved5[3];
	} __attribute__((packed));
	static_assert(sizeof(CommandCreateSQ) == 15 * sizeof(uint32_t));


	struct CommandRead
	{
		uint32_t nsid;
		uint64_t __reserved0;
		uint64_t mptr;
		DataPtr dptr;
		// dword 10-11
		uint64_t slba;
		// dword 12
		uint16_t nlb;
		uint16_t __reserved1;
		// dword 13-15
		uint32_t __reserved2[3];
	} __attribute__((packed));
	static_assert(sizeof(CommandRead) == 15 * sizeof(uint32_t));

	struct SubmissionQueueEntry
	{
		uint8_t opc;
		uint8_t fuse : 2;
		uint8_t __reserved : 4;
		uint8_t psdt : 2;
		uint16_t cid;
		union
		{
			CommandGeneric generic;
			CommandIdentify identify;
			CommandCreateCQ create_cq;
			CommandCreateSQ create_sq;
			CommandRead read;
		};
	} __attribute__((packed));
	static_assert(sizeof(SubmissionQueueEntry) == 64);

	enum OPC : uint8_t
	{
		OPC_ADMIN_CREATE_SQ = 0x01,
		OPC_ADMIN_CREATE_CQ = 0x05,
		OPC_ADMIN_IDENTIFY = 0x06,
		OPC_IO_WRITE = 0x01,
		OPC_IO_READ = 0x02,
	};

	enum CNS : uint8_t
	{
		CNS_INDENTIFY_NAMESPACE = 0x00,
		CNS_INDENTIFY_CONTROLLER = 0x01,
		CNS_INDENTIFY_ACTIVE_NAMESPACES = 0x02,
	};

	struct NamespaceIdentify
	{
		uint64_t nsze;
		uint64_t ncap;
		uint64_t nuse;
		uint8_t nsfeat;
		uint8_t nlbaf;
		uint8_t flbas;
		uint8_t mc;
		uint8_t dpc;
		uint8_t dps;
		uint8_t nmic;
		uint8_t rescap;
		uint8_t fpi;
		uint8_t dlfeat;
		uint16_t nawun;
		uint16_t nawupf;
		uint16_t nacwu;
		uint16_t nabsn;
		uint16_t nabo;
		uint16_t nabspf;
		uint16_t noiob;
		uint64_t nvmcap[2];
		uint16_t npwg;
		uint16_t npwa;
		uint16_t npdg;
		uint16_t npda;
		uint16_t nows;
		uint16_t mssrl;
		uint32_t mcl;
		uint8_t msrc;
		uint8_t __reserved0[11];
		uint32_t adagrpid;
		uint8_t __reserved1[3];
		uint8_t nsattr;
		uint16_t nvmsetid;
		uint16_t endgid;
		uint64_t nguid[2];
		uint64_t eui64;
		uint32_t lbafN[64];
		uint8_t vendor_specific[3712];
	} __attribute__((packed));
	static_assert(sizeof(NamespaceIdentify) == 0x1000);

}