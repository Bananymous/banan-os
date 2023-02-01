#include <kernel/CPUID.h>

namespace CPUID
{
	void get_cpuid(uint32_t code, uint32_t* out)
	{
		asm volatile("cpuid" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(code));
	}

	void get_cpuid_string(uint32_t code, uint32_t* out)
	{
		asm volatile ("cpuid": "=a"(out[0]), "=b"(out[0]), "=d"(out[1]), "=c"(out[2]) : "a"(code));
	}

	const char* get_vendor()
	{
		static char vendor[13] {};
		get_cpuid_string(0x00, (uint32_t*)vendor);
		vendor[12] = '\0';
		return vendor;
	}

	void get_features(uint32_t& ecx, uint32_t& edx)
	{
		uint32_t buffer[4] {};
		get_cpuid(0x01, buffer);
		ecx = buffer[2];
		edx = buffer[3];
	}

	bool is_64_bit()
	{
		uint32_t buffer[4] {};
		get_cpuid(0x80000000, buffer);
		if (buffer[0] < 0x80000001)
			return false;

		get_cpuid(0x80000001, buffer);
		return buffer[3] & (1 << 29);
	}

	const char* feature_string_ecx(uint32_t feat)
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

	const char* feature_string_edx(uint32_t feat)
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

}
