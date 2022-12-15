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

	static const char* FeatStringECX(uint32_t feat)
	{
		switch (feat)
		{
			case Features::ECX_SSE3:			return "ECX_SSE3";
			case Features::ECX_PCLMULQDQ:		return "ECX_PCLMULQDQ";
			case Features::ECX_DTES64:			return "ECX_DTES64";
			case Features::ECX_MONITOR:			return "ECX_MONITOR";
			case Features::ECX_DS_CPL:			return "ECX_DS_CPL";
			case Features::ECX_VMX:				return "ECX_VMX";
			case Features::ECX_SMX:				return "ECX_SMX";
			case Features::ECX_EST:				return "ECX_EST";
			case Features::ECX_TM2:				return "ECX_TM2";
			case Features::ECX_SSSE3:			return "ECX_SSSE3";
			case Features::ECX_CNTX_ID:			return "ECX_CNTX_ID";
			case Features::ECX_SDBG:			return "ECX_SDBG";
			case Features::ECX_FMA:				return "ECX_FMA";
			case Features::ECX_CX16:			return "ECX_CX16";
			case Features::ECX_XTPR:			return "ECX_XTPR";
			case Features::ECX_PDCM:			return "ECX_PDCM";
			case Features::ECX_PCID:			return "ECX_PCID";
			case Features::ECX_DCA:				return "ECX_DCA";
			case Features::ECX_SSE4_1:			return "ECX_SSE4_1";
			case Features::ECX_SSE4_2:			return "ECX_SSE4_2";
			case Features::ECX_X2APIC:			return "ECX_X2APIC";
			case Features::ECX_MOVBE:			return "ECX_MOVBE";
			case Features::ECX_POPCNT:			return "ECX_POPCNT";
			case Features::ECX_TSC_DEADLINE:	return "ECX_TSC_DEADLINE";
			case Features::ECX_AES:				return "ECX_AES";
			case Features::ECX_XSAVE:			return "ECX_XSAVE";
			case Features::ECX_OSXSAVE:			return "ECX_OSXSAVE";
			case Features::ECX_AVX:				return "ECX_AVX";
			case Features::ECX_F16C:			return "ECX_F16C";
			case Features::ECX_RDRND:			return "ECX_RDRND";
			case Features::ECX_HYPERVISOR:		return "ECX_HYPERVISOR";
			default: return "UNKNOWN";
		}
	}

	static const char* FeatStringEDX(uint32_t feat)
	{
		switch (feat)
		{
			case Features::EDX_FPU:		return "EDX_FPU";
			case Features::EDX_VME:		return "EDX_VME";
			case Features::EDX_DE:		return "EDX_DE";
			case Features::EDX_PSE:		return "EDX_PSE";
			case Features::EDX_TSC:		return "EDX_TSC";
			case Features::EDX_MSR:		return "EDX_MSR";
			case Features::EDX_PAE:		return "EDX_PAE";
			case Features::EDX_MCE:		return "EDX_MCE";
			case Features::EDX_CX8:		return "EDX_CX8";
			case Features::EDX_APIC:	return "EDX_APIC";
			case Features::EDX_SEP:		return "EDX_SEP";
			case Features::EDX_MTRR:	return "EDX_MTRR";
			case Features::EDX_PGE:		return "EDX_PGE";
			case Features::EDX_MCA:		return "EDX_MCA";
			case Features::EDX_CMOV:	return "EDX_CMOV";
			case Features::EDX_PAT:		return "EDX_PAT";
			case Features::EDX_PSE36:	return "EDX_PSE36";
			case Features::EDX_PSN:		return "EDX_PSN";
			case Features::EDX_CLFSH:	return "EDX_CLFSH";
			case Features::EDX_DS:		return "EDX_DS";
			case Features::EDX_ACPI:	return "EDX_ACPI";
			case Features::EDX_MMX:		return "EDX_MMX";
			case Features::EDX_FXSR:	return "EDX_FXSR";
			case Features::EDX_SSE:		return "EDX_SSE";
			case Features::EDX_SSE2:	return "EDX_SSE2";
			case Features::EDX_SS:		return "EDX_SS";
			case Features::EDX_HTT:		return "EDX_HTT";
			case Features::EDX_TM:		return "EDX_TM";
			case Features::EDX_IA64:	return "EDX_IA64";
			case Features::EDX_PBE:		return "EDX_PBE";
			default: return "NONE";
		}
	}

	bool IsAvailable();
	const char* GetVendor();
	void GetFeatures(uint32_t& ecx, uint32_t& edx);

}