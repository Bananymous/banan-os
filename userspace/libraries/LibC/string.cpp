#include <BAN/Assert.h>
#include <BAN/Math.h>
#include <BAN/Optional.h>
#include <BAN/UTF8.h>

#include <cpuid.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/weak_alias.h>

#include <emmintrin.h>

#pragma GCC optimize "no-tree-loop-distribute-patterns"

static bool s_has_ermsb { false };
static constexpr size_t s_ermsb_threshold = 1024;

__attribute__((constructor))
static void check_ermsb()
{
	unsigned int eax, ebx, ecx, edx;
	if (!__get_cpuid(7, &eax, &ebx, &ecx, &edx))
		return;
	s_has_ermsb = !!(ebx & (1 << 9));
}

void* memset(void* s, int c, size_t n)
{
	uint8_t* dst_u8 = static_cast<uint8_t*>(s);

	if (s_has_ermsb && n >= s_ermsb_threshold)
	{
		asm volatile(
			"rep stosb"
			: "+D"(dst_u8), "+c"(n)
			: "a"(c)
			: "memory"
		);
		return s;
	}

	const uint8_t byte = c;
	const __m128i value = _mm_set1_epi8(byte);

	if (const size_t rem = reinterpret_cast<uintptr_t>(dst_u8) % 64; rem && n >= 64)
	{
		for (size_t i = 0; i * 16 < 64 - rem; i++)
			_mm_storeu_si128(reinterpret_cast<__m128i*>(dst_u8) + i, value);
		dst_u8 += 64 - rem;
		n      -= 64 - rem;
	}

	for (; n >= 64; n -= 64, dst_u8 += 64)
	{
		__m128i* dst = reinterpret_cast<__m128i*>(dst_u8);
		_mm_store_si128(dst + 0, value);
		_mm_store_si128(dst + 1, value);
		_mm_store_si128(dst + 2, value);
		_mm_store_si128(dst + 3, value);
	}

	for (size_t i = 0; i < n; i++)
		dst_u8[i] = byte;

	return s;
}

void* memcpy(void* __restrict__ s1, const void* __restrict__ s2, size_t n)
{
	      uint8_t* dst_u8 = static_cast<      uint8_t*>(s1);
	const uint8_t* src_u8 = static_cast<const uint8_t*>(s2);

	if (s_has_ermsb && n >= s_ermsb_threshold)
	{
		asm volatile(
			"rep movsb"
			: "+D"(dst_u8), "+S"(src_u8), "+c"(n)
			:
			: "memory"
		);
		return s1;
	}

	if (const size_t rem = reinterpret_cast<uintptr_t>(dst_u8) % 64; rem && n >= 64)
	{
		const size_t bytes = 64 - rem;
		for (size_t i = 0; i < bytes; i++)
			*dst_u8++ = *src_u8++;
		n -= bytes;
	}

	for (; n >= 64; n -= 64, dst_u8 += 64, src_u8 += 64)
	{
		      __m128i* dst = reinterpret_cast<      __m128i*>(dst_u8);
		const __m128i* src = reinterpret_cast<const __m128i*>(src_u8);

		const __m128i value0 = _mm_loadu_si128(src + 0);
		const __m128i value1 = _mm_loadu_si128(src + 1);
		const __m128i value2 = _mm_loadu_si128(src + 2);
		const __m128i value3 = _mm_loadu_si128(src + 3);

		_mm_store_si128(dst + 0, value0);
		_mm_store_si128(dst + 1, value1);
		_mm_store_si128(dst + 2, value2);
		_mm_store_si128(dst + 3, value3);
	}

	for (; n > 0; n--)
		*dst_u8++ = *src_u8++;

	return s1;
}

void* memmove(void* s1, const void* s2, size_t n)
{
	if (s1 < s2)
		return memcpy(s1, s2, n);

	      uint8_t* dst_u8 = static_cast<      uint8_t*>(s1) + n;
	const uint8_t* src_u8 = static_cast<const uint8_t*>(s2) + n;

	if (s_has_ermsb && n >= s_ermsb_threshold)
	{
		asm volatile(
			"std; rep movsb; cld"
			: "+D"(--dst_u8), "+S"(--src_u8), "+c"(n)
			:
			: "memory"
		);
		return s1;
	}

	if (const size_t rem = reinterpret_cast<uintptr_t>(dst_u8) % 64; rem && n >= 64)
	{
		const size_t bytes = rem;
		for (size_t i = 0; i < bytes; i++)
			*--dst_u8 = *--src_u8;
		n -= bytes;
	}

	for (; n >= 64; n -= 64, dst_u8 -= 64, src_u8 -= 64)
	{
		      __m128i* dst = reinterpret_cast<      __m128i*>(dst_u8 - 64);
		const __m128i* src = reinterpret_cast<const __m128i*>(src_u8 - 64);

		const __m128i value0 = _mm_loadu_si128(src + 0);
		const __m128i value1 = _mm_loadu_si128(src + 1);
		const __m128i value2 = _mm_loadu_si128(src + 2);
		const __m128i value3 = _mm_loadu_si128(src + 3);

		_mm_store_si128(dst + 0, value0);
		_mm_store_si128(dst + 1, value1);
		_mm_store_si128(dst + 2, value2);
		_mm_store_si128(dst + 3, value3);
	}

	for (; n > 0; n--)
		*--dst_u8 = *--src_u8;

	return s1;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
	const uint8_t* src1_u8 = static_cast<const uint8_t*>(s1);
	const uint8_t* src2_u8 = static_cast<const uint8_t*>(s2);

	for (; n >= 16; n -= 16, src1_u8 += 16, src2_u8 += 16)
	{
		const __m128i* src1 = reinterpret_cast<const __m128i*>(src1_u8);
		const __m128i* src2 = reinterpret_cast<const __m128i*>(src2_u8);

		const __m128i comp = _mm_cmpeq_epi8(_mm_loadu_si128(src1), _mm_loadu_si128(src2));
		if (const uint16_t mask = _mm_movemask_epi8(comp) ^ 0xFFFF)
		{
			const size_t diff_bit = BAN::Math::ctz(mask);
			return src1_u8[diff_bit] - src2_u8[diff_bit];
		}
	}

	for (size_t i = 0; i < n; i++)
		if (src1_u8[i] != src2_u8[i])
			return src1_u8[i] - src2_u8[i];

	return 0;
}

void* memchr(const void* s, int c, size_t n)
{
	const uint8_t* src_u8 = static_cast<const uint8_t*>(s);

	const __m128i equal = _mm_set1_epi8(c);

	// NOTE: align to not cross into an invalid page boundary on last load
	if (const size_t rem = reinterpret_cast<uintptr_t>(src_u8) % 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(src_u8 - rem));
		if (const uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)) >> rem)
			if (const size_t bit = BAN::Math::ctz(mask); bit < n)
				return const_cast<uint8_t*>(src_u8 + bit);

		if (n <= 16 - rem)
			return nullptr;
		src_u8 += 16 - rem;
		n      -= 16 - rem;
	}

	for (; n >= 16; n -= 16, src_u8 += 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(src_u8));
		if (const uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)))
			return const_cast<uint8_t*>(src_u8 + BAN::Math::ctz(mask));
	}

	if (n > 0)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(src_u8));
		if (const uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)))
			if (const size_t bit = BAN::Math::ctz(mask); bit < n)
				return const_cast<uint8_t*>(src_u8 + bit);
	}

	return nullptr;
}

void* memccpy(void* __restrict s1, const void* __restrict s2, int c, size_t n)
{
	      uint8_t* dst_u8 = static_cast<      uint8_t*>(s1);
	const uint8_t* src_u8 = static_cast<const uint8_t*>(s2);

	const uint8_t byte = c;
	const __m128i equal = _mm_set1_epi8(byte);

	// NOTE: align src so we don't load from `s2` past `c`
	if (const auto rem = reinterpret_cast<uintptr_t>(src_u8) % 16; rem && n >= 16)
	{
		for (size_t i = 0; i < 16 - rem; i++)
			if ((dst_u8[i] = src_u8[i]) == byte)
				return dst_u8 + i + 1;
		dst_u8 += 16 - rem;
		src_u8 += 16 - rem;
		n      -= 16 - rem;
	}

	for (; n >= 16; n -= 16, dst_u8 += 16, src_u8 += 16)
	{
		      __m128i* dst = reinterpret_cast<      __m128i*>(dst_u8);
		const __m128i* src = reinterpret_cast<const __m128i*>(src_u8);

		const __m128i value = _mm_load_si128(src);
		if (_mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)))
			break;

		_mm_storeu_si128(dst, value);
	}

	for (size_t i = 0; i < n; i++)
		if ((dst_u8[i] = src_u8[i]) == byte)
			return dst_u8 + i + 1;

	return nullptr;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
	const auto handle_page_boundary = [&s1, &s2]() -> BAN::Optional<int> {
		const size_t rem_s1 = reinterpret_cast<uintptr_t>(s1) & 0xFFF;
		const size_t rem_s2 = reinterpret_cast<uintptr_t>(s2) & 0xFFF;
		if (rem_s1 <= 0xFF0 && rem_s2 <= 0xFF0)
			return {};

		const size_t bytes = BAN::Math::min(0x1000 - rem_s1, 0x1000 - rem_s2);
		for (size_t i = 0; i < bytes; i++, s1++, s2++)
			if (*s1 == '\0' || *s2 == '\0' || *s1 != *s2)
				return *s1 - *s2;

		return {};
	};

	const __m128i zero = _mm_setzero_si128();

	for (; n >= 16; n -= 16, s1 += 16, s2 += 16)
	{
		if (const auto ret = handle_page_boundary(); ret.has_value())
			return ret.value();

		const __m128i value1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s1));
		const __m128i value2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s2));

		const uint16_t mask_zero1 = _mm_movemask_epi8(_mm_cmpeq_epi8(value1, zero));
		const uint16_t mask_zero2 = _mm_movemask_epi8(_mm_cmpeq_epi8(value2, zero));
		const uint16_t mask_equal = _mm_movemask_epi8(_mm_cmpeq_epi8(value1, value2));
		if (const uint16_t mask = mask_zero1 | mask_zero2 | (mask_equal ^ 0xFFFF))
		{
			const size_t diff_bit = BAN::Math::ctz(mask);
			return s1[diff_bit] - s2[diff_bit];
		}
	}

	for (size_t i = 0; i < n; i++, s1++, s2++)
		if (*s1 == '\0' || *s2 == '\0' || *s1 != *s2)
			return *s1 - *s2;

	return 0;
}

char* strchrnul(const char* s, int c)
{
	if (static_cast<char>(c) == '\0')
		return const_cast<char*>(s) + strlen(s);

	const __m128i equal = _mm_set1_epi8(c);
	const __m128i zero = _mm_setzero_si128();

	// NOTE: align to not cross into an invalid page boundary on last load
	if (const size_t rem = reinterpret_cast<uintptr_t>(s) % 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(s - rem));

		const uint16_t mask_zero  = _mm_movemask_epi8(_mm_cmpeq_epi8(value, zero )) >> rem;
		const uint16_t mask_equal = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)) >> rem;
		if (const uint16_t mask = mask_zero | mask_equal)
			return const_cast<char*>(s + BAN::Math::ctz(mask));

		s += 16 - rem;
	}

	for (;; s += 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(s));

		const uint16_t mask_zero  = _mm_movemask_epi8(_mm_cmpeq_epi8(value, zero ));
		const uint16_t mask_equal = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal));
		if (const uint16_t mask = mask_zero | mask_equal)
			return const_cast<char*>(s + BAN::Math::ctz(mask));
	}
}

char* strrchr(const char* s, int c)
{
	if (static_cast<char>(c) == '\0')
		return const_cast<char*>(s + strlen(s));

	const __m128i equal = _mm_set1_epi8(c);
	const __m128i zero = _mm_setzero_si128();

	const char* last = nullptr;

	// NOTE: align to not cross into an invalid page boundary on last load
	if (const size_t rem = reinterpret_cast<uintptr_t>(s) % 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(s - rem));

		const uint16_t mask_zero  = _mm_movemask_epi8(_mm_cmpeq_epi8(value, zero )) >> rem;
		      uint16_t mask_equal = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal)) >> rem;

		if (mask_zero != 0)
		{
			mask_equal &= (1u << BAN::Math::ctz(mask_zero)) - 1;
			if (mask_equal != 0)
				last = s + 16 - BAN::Math::clz(mask_equal) - 1;
			return const_cast<char*>(last);
		}

		if (mask_equal != 0)
			last = s + 16 - BAN::Math::clz<uint16_t>(mask_equal) - 1;

		s += 16 - rem;
	}

	for (;; s += 16)
	{
		const __m128i value = _mm_load_si128(reinterpret_cast<const __m128i*>(s));

		const uint16_t mask_zero  = _mm_movemask_epi8(_mm_cmpeq_epi8(value, zero ));
		      uint16_t mask_equal = _mm_movemask_epi8(_mm_cmpeq_epi8(value, equal));

		if (mask_zero != 0)
		{
			mask_equal &= (1u << BAN::Math::ctz(mask_zero)) - 1;
			if (mask_equal != 0)
				last = s + 16 - BAN::Math::clz(mask_equal) - 1;
			return const_cast<char*>(last);
		}

		if (mask_equal != 0)
			last = s + 16 - BAN::Math::clz<uint16_t>(mask_equal) - 1;
	}
}

int strcmp(const char* s1, const char* s2)
{
	return strncmp(s1, s2, -1);
}

char* stpcpy(char* __restrict__ s1, const char* __restrict__ s2)
{
	return static_cast<char*>(memccpy(s1, s2, '\0', -1)) - 1;
}

char* stpncpy(char* __restrict__ s1, const char* __restrict__ s2, size_t n)
{
	char* end = static_cast<char*>(memccpy(s1, s2, '\0', n));
	if (end == nullptr)
		return s1 + n;
	memset(end, '\0', n - (end - s1));
	return end - 1;
}

char* strcpy(char* __restrict__ s1, const char* __restrict__ s2)
{
	stpcpy(s1, s2);
	return s1;
}

char* strncpy(char* __restrict__ s1, const char* __restrict__ s2, size_t n)
{
	stpncpy(s1, s2, n);
	return s1;
}

char* strcat(char* __restrict__ s1, const char* __restrict__ s2)
{
	stpcpy(s1 + strlen(s1), s2);
	return s1;
}

char* strncat(char* __restrict__ s1, const char* __restrict__ s2, size_t n)
{
	char* dst = s1 + strlen(s1);
	if (memccpy(dst, s2, '\0', n) == nullptr)
		dst[n] = '\0';
	return s1;
}

char* strdup(const char* str)
{
	const size_t size = strlen(str);

	char* new_str = static_cast<char*>(malloc(size + 1));
	if (new_str == nullptr)
		return nullptr;

	memcpy(new_str, str, size + 1);
	return new_str;
}

char* strndup(const char* str, size_t size)
{
	size = strnlen(str, size);

	char* new_str = static_cast<char*>(malloc(size + 1));
	if (new_str == nullptr)
		return nullptr;

	memcpy(new_str, str, size);
	new_str[size] = '\0';
	return new_str;
}

size_t strlen(const char* str)
{
	return static_cast<const char*>(memchr(str, '\0', -1)) - str;
}

size_t strnlen(const char* str, size_t maxlen)
{
	const char* null = static_cast<const char*>(memchr(str, '\0', maxlen));
	if (null == nullptr)
		return maxlen;
	return null - str;
}

char* strchr(const char* str, int c)
{
	if (c == '\0')
		return const_cast<char*>(str + strlen(str));
	char* result = strchrnul(str, c);
	return *result ? result : nullptr;
}

char* strsep(char** __restrict stringp, const char* __restrict delim)
{
	if (*stringp == nullptr)
		return nullptr;

	char* original = *stringp;

	char* match = strpbrk(*stringp, delim);
	if (match == nullptr)
		*stringp = nullptr;
	else
	{
		*stringp = match + 1;
		*match = '\0';
	}

	return original;
}

char* strstr(const char* haystack, const char* needle)
{
	const size_t needle_len = strlen(needle);
	if (needle_len == 0)
		return const_cast<char*>(haystack);
	for (size_t i = 0; haystack[i]; i++)
		if (strncmp(haystack + i, needle, needle_len) == 0)
			return const_cast<char*>(haystack + i);
	return nullptr;
}

char* strcasestr(const char* haystack, const char* needle)
{
	const size_t needle_len = strlen(needle);
	if (needle_len == 0)
		return const_cast<char*>(haystack);
	for (size_t i = 0; haystack[i]; i++)
		if (strncasecmp(haystack + i, needle, needle_len) == 0)
			return const_cast<char*>(haystack + i);
	return nullptr;
}

int strcoll(const char* s1, const char* s2)
{
	return strcoll_l(s1, s2, __getlocale(LC_COLLATE));
}

int strcoll_l(const char *s1, const char *s2, locale_t locale)
{
	switch (locale->encoding)
	{
		case __ENC_ASCII:
			return strcmp(s1, s2);
		case __ENC_UTF8:
		{
			const unsigned char* u1 = (unsigned char*)s1;
			const unsigned char* u2 = (unsigned char*)s2;
			if (!*u1 || !*u2)
				return *u1 - *u2;

			wchar_t wc1, wc2;
			while (*u1 && *u2)
			{
				wc1 = BAN::UTF8::to_codepoint(u1);
				wc2 = BAN::UTF8::to_codepoint(u2);
				if (wc1 == (wchar_t)BAN::UTF8::invalid || wc2 == (wchar_t)BAN::UTF8::invalid)
				{
					errno = EINVAL;
					return -1;
				}
				if (wc1 != wc2)
					break;
				u1 += BAN::UTF8::byte_length(*u1);
				u2 += BAN::UTF8::byte_length(*u2);
			}

			// TODO: this isn't really correct :D
			return wc1 - wc2;
		}
	}

	ASSERT_NOT_REACHED();
}

#define CHAR_UCHAR(ch) \
	static_cast<unsigned char>(ch)

#define CHAR_BITMASK(str) \
	uint32_t bitmask[0x100 / 32] {}; \
	for (size_t i = 0; str[i]; i++) \
		bitmask[CHAR_UCHAR(str[i]) / 32] |= (1 << (CHAR_UCHAR(str[i]) % 32))

#define CHAR_BITMASK_TEST(ch) \
	(bitmask[CHAR_UCHAR(ch) / 32] & (1 << (CHAR_UCHAR(ch) % 32)))

char* strpbrk(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0; s1[i]; i++)
		if (CHAR_BITMASK_TEST(s1[i]))
			return const_cast<char*>(&s1[i]);
	return nullptr;
}

size_t strspn(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0;; i++)
		if (s1[i] == '\0' || !CHAR_BITMASK_TEST(s1[i]))
			return i;
}

size_t strcspn(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0;; i++)
		if (s1[i] == '\0' || CHAR_BITMASK_TEST(s1[i]))
			return i;
}

char* strtok(char* __restrict s, const char* __restrict sep)
{
	static char* state = nullptr;
	return strtok_r(s, sep, &state);
}

char* strtok_r(char* __restrict str, const char* __restrict sep, char** __restrict state)
{
	CHAR_BITMASK(sep);

	if (str)
		*state = str;

	if (*state == nullptr)
		return nullptr;

	str = *state;

	for (; *str; str++)
		if (!CHAR_BITMASK_TEST(*str))
			break;

	if (*str == '\0')
	{
		*state = nullptr;
		return nullptr;
	}

	for (size_t i = 0; str[i]; i++)
	{
		if (!CHAR_BITMASK_TEST(str[i]))
			continue;
		str[i] = '\0';
		*state = str + i + 1;
		return str;
	}

	*state = nullptr;
	return str;
}

#undef CHAR_UCHAR
#undef CHAR_BITMASK
#undef CHAR_BITMASK_TEST

size_t strxfrm(char* __restrict s1, const char* __restrict s2, size_t n)
{
	return strxfrm_l(s1, s2, n, __getlocale(LC_COLLATE));
}

size_t strxfrm_l(char* __restrict s1, const char* __restrict s2, size_t n, locale_t locale)
{
	(void)locale;
	// TODO: this isn't really correct :D
	strncpy(s1, s2, n);
	return strlen(s2);
}

char* strsignal(int signum)
{
	static char buffer[128];
	switch (signum)
	{
		case SIGABRT:	strcpy(buffer, "Process abort signal."); break;
		case SIGALRM:	strcpy(buffer, "Alarm clock."); break;
		case SIGBUS:	strcpy(buffer, "Access to an undefined portion of a memory object."); break;
		case SIGCHLD:	strcpy(buffer, "Child process terminated, stopped, or continued."); break;
		case SIGCONT:	strcpy(buffer, "Continue executing, if stopped."); break;
		case SIGFPE:	strcpy(buffer, "Erroneous arithmetic operation."); break;
		case SIGHUP:	strcpy(buffer, "Hangup."); break;
		case SIGILL:	strcpy(buffer, "Illegal instruction."); break;
		case SIGINT:	strcpy(buffer, "Terminal interrupt signal."); break;
		case SIGKILL:	strcpy(buffer, "Kill (cannot be caught or ignored)."); break;
		case SIGPIPE:	strcpy(buffer, "Write on a pipe with no one to read it."); break;
		case SIGQUIT:	strcpy(buffer, "Terminal quit signal."); break;
		case SIGSEGV:	strcpy(buffer, "Invalid memory reference."); break;
		case SIGSTOP:	strcpy(buffer, "Stop executing (cannot be caught or ignored)."); break;
		case SIGTERM:	strcpy(buffer, "Termination signal."); break;
		case SIGTSTP:	strcpy(buffer, "Terminal stop signal."); break;
		case SIGTTIN:	strcpy(buffer, "Background process attempting read."); break;
		case SIGTTOU:	strcpy(buffer, "Background process attempting write."); break;
		case SIGUSR1:	strcpy(buffer, "User-defined signal 1."); break;
		case SIGUSR2:	strcpy(buffer, "User-defined signal 2."); break;
		case SIGPOLL:	strcpy(buffer, "Pollable event."); break;
		case SIGPROF:	strcpy(buffer, "Profiling timer expired."); break;
		case SIGSYS:	strcpy(buffer, "Bad system call."); break;
		case SIGTRAP:	strcpy(buffer, "Trace/breakpoint trap."); break;
		case SIGURG:	strcpy(buffer, "High bandwidth data is available at a socket."); break;
		case SIGVTALRM:	strcpy(buffer, "Virtual timer expired."); break;
		case SIGXCPU:	strcpy(buffer, "CPU time limit exceeded."); break;
		case SIGXFSZ:	strcpy(buffer, "File size limit exceeded."); break;
	}
	return buffer;
}

char* strerror(int error)
{
	static char buffer[128];
	if (const char* str = strerrordesc_np(error))
		strcpy(buffer, str);
	else
		sprintf(buffer, "Unknown error %d", error);
	return buffer;
}

int strerror_r(int error, char* strerrbuf, size_t buflen)
{
	const char* str = strerrordesc_np(error);
	if (!str)
		return EINVAL;
	if (strlen(str) + 1 > buflen)
		return ERANGE;
	strcpy(strerrbuf, str);
	return 0;
}

const char* strerrorname_np(int error)
{
	switch (error)
	{
		case 0:					return "NOERROR";
		case E2BIG:				return "E2BIG";
		case EACCES:			return "EACCES";
		case EADDRINUSE:		return "EADDRINUSE";
		case EADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case EAFNOSUPPORT:		return "EAFNOSUPPORT";
		case EAGAIN:			return "EAGAIN";
		case EALREADY:			return "EALREADY";
		case EBADF:				return "EBADF";
		case EBADMSG:			return "EBADMSG";
		case EBUSY:				return "EBUSY";
		case ECANCELED:			return "ECANCELED";
		case ECHILD:			return "ECHILD";
		case ECONNABORTED:		return "ECONNABORTED";
		case ECONNREFUSED:		return "ECONNREFUSED";
		case ECONNRESET:		return "ECONNRESET";
		case EDEADLK:			return "EDEADLK";
		case EDESTADDRREQ:		return "EDESTADDRREQ";
		case EDOM:				return "EDOM";
		case EDQUOT:			return "EDQUOT";
		case EEXIST:			return "EEXIST";
		case EFAULT:			return "EFAULT";
		case EFBIG:				return "EFBIG";
		case EHOSTUNREACH:		return "EHOSTUNREACH";
		case EIDRM:				return "EIDRM";
		case EILSEQ:			return "EILSEQ";
		case EINPROGRESS:		return "EINPROGRESS";
		case EINTR:				return "EINTR";
		case EINVAL:			return "EINVAL";
		case EIO:				return "EIO";
		case EISCONN:			return "EISCONN";
		case EISDIR:			return "EISDIR";
		case ELOOP:				return "ELOOP";
		case EMFILE:			return "EMFILE";
		case EMLINK:			return "EMLINK";
		case EMSGSIZE:			return "EMSGSIZE";
		case EMULTIHOP:			return "EMULTIHOP";
		case ENAMETOOLONG:		return "ENAMETOOLONG";
		case ENETDOWN:			return "ENETDOWN";
		case ENETRESET:			return "ENETRESET";
		case ENETUNREACH:		return "ENETUNREACH";
		case ENFILE:			return "ENFILE";
		case ENOBUFS:			return "ENOBUFS";
		case ENODATA:			return "ENODATA";
		case ENODEV:			return "ENODEV";
		case ENOENT:			return "ENOENT";
		case ENOEXEC:			return "ENOEXEC";
		case ENOLCK:			return "ENOLCK";
		case ENOLINK:			return "ENOLINK";
		case ENOMEM:			return "ENOMEM";
		case ENOMSG:			return "ENOMSG";
		case ENOPROTOOPT:		return "ENOPROTOOPT";
		case ENOSPC:			return "ENOSPC";
		case ENOSR:				return "ENOSR";
		case ENOSTR:			return "ENOSTR";
		case ENOSYS:			return "ENOSYS";
		case ENOTCONN:			return "ENOTCONN";
		case ENOTDIR:			return "ENOTDIR";
		case ENOTEMPTY:			return "ENOTEMPTY";
		case ENOTRECOVERABLE:	return "ENOTRECOVERABLE";
		case ENOTSOCK:			return "ENOTSOCK";
		case ENOTSUP:			return "ENOTSUP";
		case ENOTTY:			return "ENOTTY";
		case ENXIO:				return "ENXIO";
		case EOPNOTSUPP:		return "EOPNOTSUPP";
		case EOVERFLOW:			return "EOVERFLOW";
		case EOWNERDEAD:		return "EOWNERDEAD";
		case EPERM:				return "EPERM";
		case EPIPE:				return "EPIPE";
		case EPROTO:			return "EPROTO";
		case EPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case EPROTOTYPE:		return "EPROTOTYPE";
		case ERANGE:			return "ERANGE";
		case EROFS:				return "EROFS";
		case ESPIPE:			return "ESPIPE";
		case ESRCH:				return "ESRCH";
		case ESTALE:			return "ESTALE";
		case ETIME:				return "ETIME";
		case ETIMEDOUT:			return "ETIMEDOUT";
		case ETXTBSY:			return "ETXTBSY";
		case EWOULDBLOCK:		return "EWOULDBLOCK";
		case EXDEV:				return "EXDEV";
		case ENOTBLK:			return "ENOTBLK";
		case ESHUTDOWN:			return "ESHUTDOWN";
		case EUNKNOWN:			return "EUNKNOWN";
	}

	errno = EINVAL;
	return nullptr;
}

const char* strerrordesc_np(int error)
{
	switch (error)
	{
		case 0:					return "Success";
		case E2BIG:				return "Argument list too long.";
		case EACCES:			return "Permission denied.";
		case EADDRINUSE:		return "Address in use.";
		case EADDRNOTAVAIL:		return "Address not available.";
		case EAFNOSUPPORT:		return "Address family not supported.";
		case EAGAIN:			return "Resource unavailable, try again.";
		case EALREADY:			return "Connection already in progress.";
		case EBADF:				return "Bad file descriptor.";
		case EBADMSG:			return "Bad message.";
		case EBUSY:				return "Device or resource busy.";
		case ECANCELED:			return "Operation canceled.";
		case ECHILD:			return "No child processes.";
		case ECONNABORTED:		return "Connection aborted.";
		case ECONNREFUSED:		return "Connection refused.";
		case ECONNRESET:		return "Connection reset.";
		case EDEADLK:			return "Resource deadlock would occur.";
		case EDESTADDRREQ:		return "Destination address required.";
		case EDOM:				return "Mathematics argument out of domain of function.";
		case EDQUOT:			return "Reserved.";
		case EEXIST:			return "File exists.";
		case EFAULT:			return "Bad address.";
		case EFBIG:				return "File too large.";
		case EHOSTUNREACH:		return "Host is unreachable.";
		case EIDRM:				return "Identifier removed.";
		case EILSEQ:			return "Illegal byte sequence.";
		case EINPROGRESS:		return "Operation in progress.";
		case EINTR:				return "Interrupted function.";
		case EINVAL:			return "Invalid argument.";
		case EIO:				return "I/O error.";
		case EISCONN:			return "Socket is connected.";
		case EISDIR:			return "Is a directory.";
		case ELOOP:				return "Too many levels of symbolic links.";
		case EMFILE:			return "File descriptor value too large.";
		case EMLINK:			return "Too many links.";
		case EMSGSIZE:			return "Message too large.";
		case EMULTIHOP:			return "Reserved.";
		case ENAMETOOLONG:		return "Filename too long.";
		case ENETDOWN:			return "Network is down.";
		case ENETRESET:			return "Connection aborted by network.";
		case ENETUNREACH:		return "Network unreachable.";
		case ENFILE:			return "Too many files open in system.";
		case ENOBUFS:			return "No buffer space available.";
		case ENODATA:			return "No message is available on the STREAM head read queue.";
		case ENODEV:			return "No such device.";
		case ENOENT:			return "No such file or directory.";
		case ENOEXEC:			return "Executable file format error.";
		case ENOLCK:			return "No locks available.";
		case ENOLINK:			return "Reserved.";
		case ENOMEM:			return "Not enough space.";
		case ENOMSG:			return "No message of the desired type.";
		case ENOPROTOOPT:		return "Protocol not available.";
		case ENOSPC:			return "No space left on device.";
		case ENOSR:				return "No STREAM resources.";
		case ENOSTR:			return "Not a STREAM.";
		case ENOSYS:			return "Functionality not supported.";
		case ENOTCONN:			return "The socket is not connected.";
		case ENOTDIR:			return "Not a directory or a symbolic link to a directory.";
		case ENOTEMPTY:			return "Directory not empty.";
		case ENOTRECOVERABLE:	return "State not recoverable.";
		case ENOTSOCK:			return "Not a socket.";
		case ENOTSUP:			return "Not supported.";
		case ENOTTY:			return "Inappropriate I/O control operation.";
		case ENXIO:				return "No such device or address.";
		case EOPNOTSUPP:		return "Operation not supported on socket .";
		case EOVERFLOW:			return "Value too large to be stored in data type.";
		case EOWNERDEAD:		return "Previous owner died.";
		case EPERM:				return "Operation not permitted.";
		case EPIPE:				return "Broken pipe.";
		case EPROTO:			return "Protocol error.";
		case EPROTONOSUPPORT:	return "Protocol not supported.";
		case EPROTOTYPE:		return "Protocol wrong type for socket.";
		case ERANGE:			return "Result too large.";
		case EROFS:				return "Read-only file system.";
		case ESPIPE:			return "Invalid seek.";
		case ESRCH:				return "No such process.";
		case ESTALE:			return "Reserved.";
		case ETIME:				return "Stream ioctl() timeout.";
		case ETIMEDOUT:			return "Connection timed out.";
		case ETXTBSY:			return "Text file busy.";
		case EWOULDBLOCK:		return "Operation would block.";
		case EXDEV:				return "Cross-device link.";
		case ENOTBLK:			return "Block device required";
		case ESHUTDOWN:			return "Cannot send after transport endpoint shutdown.";
		case EUNKNOWN:			return "Unknown error";
	}

	errno = EINVAL;
	return nullptr;
}
