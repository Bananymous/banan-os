#pragma once

#include <stdint.h>

namespace CPUID
{

	enum Features : uint32_t
	{
		ECX_SSE3			= (uint32_t)1 << 0,
		ECX_PCLMULQDQ		= (uint32_t)1 << 1,
		ECX_DTES64			= (uint32_t)1 << 2,
		ECX_MONITOR			= (uint32_t)1 << 3,
		ECX_DS_CPL			= (uint32_t)1 << 4,
		ECX_VMX				= (uint32_t)1 << 5,
		ECX_SMX				= (uint32_t)1 << 6,
		ECX_EST				= (uint32_t)1 << 7,
		ECX_TM2				= (uint32_t)1 << 8,
		ECX_SSSE3			= (uint32_t)1 << 9,
		ECX_CNTX_ID			= (uint32_t)1 << 10,
		ECX_SDBG			= (uint32_t)1 << 11,
		ECX_FMA				= (uint32_t)1 << 12,
		ECX_CX16			= (uint32_t)1 << 13,
		ECX_XTPR			= (uint32_t)1 << 14,
		ECX_PDCM			= (uint32_t)1 << 15,
		ECX_PCID			= (uint32_t)1 << 17,
		ECX_DCA				= (uint32_t)1 << 18,
		ECX_SSE4_1			= (uint32_t)1 << 19,
		ECX_SSE4_2			= (uint32_t)1 << 20,
		ECX_X2APIC			= (uint32_t)1 << 21,
		ECX_MOVBE			= (uint32_t)1 << 22,
		ECX_POPCNT			= (uint32_t)1 << 23,
		ECX_TSC_DEADLINE	= (uint32_t)1 << 24,
		ECX_AES				= (uint32_t)1 << 25,
		ECX_XSAVE			= (uint32_t)1 << 26,
		ECX_OSXSAVE			= (uint32_t)1 << 27,
		ECX_AVX				= (uint32_t)1 << 28,
		ECX_F16C			= (uint32_t)1 << 29,
		ECX_RDRND			= (uint32_t)1 << 30,
		ECX_HYPERVISOR		= (uint32_t)1 << 31,

		EDX_FPU				= (uint32_t)1 << 0,
		EDX_VME				= (uint32_t)1 << 1,
		EDX_DE				= (uint32_t)1 << 2,
		EDX_PSE				= (uint32_t)1 << 3,
		EDX_TSC				= (uint32_t)1 << 4,
		EDX_MSR				= (uint32_t)1 << 5,
		EDX_PAE				= (uint32_t)1 << 6,
		EDX_MCE				= (uint32_t)1 << 7,
		EDX_CX8				= (uint32_t)1 << 8,
		EDX_APIC			= (uint32_t)1 << 9,
		EDX_SEP				= (uint32_t)1 << 11,
		EDX_MTRR			= (uint32_t)1 << 12,
		EDX_PGE				= (uint32_t)1 << 13,
		EDX_MCA				= (uint32_t)1 << 14,
		EDX_CMOV			= (uint32_t)1 << 15,
		EDX_PAT				= (uint32_t)1 << 16,
		EDX_PSE36			= (uint32_t)1 << 17,
		EDX_PSN				= (uint32_t)1 << 18,
		EDX_CLFSH			= (uint32_t)1 << 19,
		EDX_DS				= (uint32_t)1 << 21,
		EDX_ACPI			= (uint32_t)1 << 22,
		EDX_MMX				= (uint32_t)1 << 23,
		EDX_FXSR			= (uint32_t)1 << 24,
		EDX_SSE				= (uint32_t)1 << 25,
		EDX_SSE2			= (uint32_t)1 << 26,
		EDX_SS				= (uint32_t)1 << 27,
		EDX_HTT				= (uint32_t)1 << 28,
		EDX_TM				= (uint32_t)1 << 29,
		EDX_IA64			= (uint32_t)1 << 30,
		EDX_PBE				= (uint32_t)1 << 31,
	};

	const char* feature_string_ecx(uint32_t feat);
	const char* feature_string_edx(uint32_t feat);

	const char* get_vendor();
	void get_features(uint32_t& ecx, uint32_t& edx);
	bool is_64_bit();
	bool has_nxe();
	bool has_pge();
	bool has_pat();

}
