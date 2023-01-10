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

	const char* GetVendor()
	{
		static char vendor[13] {};
		get_cpuid_string(0x00, (uint32_t*)vendor);
		vendor[12] = '\0';
		return vendor;
	}

	void GetFeatures(uint32_t& ecx, uint32_t& edx)
	{
		uint32_t buffer[4] {};
		get_cpuid(0x01, buffer);
		ecx = buffer[2];
		edx = buffer[3];
	}

	bool Is64Bit()
	{
		uint32_t buffer[4] {};
		get_cpuid(0x80000000, buffer);
		if (buffer[0] < 0x80000001)
			return false;

		get_cpuid(0x80000001, buffer);
		return buffer[3] & (1 << 29);
	}

}
