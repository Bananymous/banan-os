#include <BAN/Assert.h>
#include <BAN/Limits.h>
#include <BAN/Math.h>
#include <BAN/UTF8.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <icxxabi.h>

void abort(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGABRT);
	sigprocmask(SIG_UNBLOCK, &set, nullptr);
	raise(SIGABRT);

	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);

	ASSERT_NOT_REACHED();
}

void exit(int status)
{
	__cxa_finalize(nullptr);
	fflush(nullptr);
	_exit(status);
	ASSERT_NOT_REACHED();
}

void _Exit(int status)
{
	_exit(status);
}

int abs(int val)
{
	return val < 0 ? -val : val;
}

int atexit(void (*func)(void))
{
	void* func_addr = reinterpret_cast<void*>(func);
	return __cxa_atexit([](void* func_ptr) { reinterpret_cast<void (*)(void)>(func_ptr)(); }, func_addr, nullptr);
}

static constexpr int get_base_digit(char c, int base)
{
	int digit = -1;
	if (isdigit(c))
		digit = c - '0';
	else if (isalpha(c))
		digit = 10 + tolower(c) - 'a';
	if (digit < base)
		return digit;
	return -1;
}

template<BAN::integral T>
static constexpr bool will_digit_append_overflow(T current, int digit, int base)
{
	if (BAN::Math::will_multiplication_overflow<T>(current, base))
		return true;
	if (BAN::Math::will_addition_overflow<T>(current * base, current < 0 ? -digit : digit))
		return true;
	return false;
}

template<BAN::integral T>
static T strtoT(const char* str, char** endp, int base, int& error)
{
	// validate base
	if (base != 0 && (base < 2 || base > 36))
	{
		if (endp)
			*endp = const_cast<char*>(str);
		error = EINVAL;
		return 0;
	}

	// skip whitespace
	while (isspace(*str))
		str++;

	// get sign and skip it
	bool negative = (*str == '-');
	if (*str == '-' || *str == '+')
		str++;

	// determine base from prefix
	if (base == 0)
	{
		if (strncasecmp(str, "0x", 2) == 0)
			base = 16;
		else if (*str == '0')
			base = 8;
		else if (isdigit(*str))
			base = 10;
	}

	// check for invalid conversion
	if (get_base_digit(*str, base) == -1)
	{
		if (endp)
			*endp = const_cast<char*>(str);
		error = EINVAL;
		return 0;
	}

	// remove "0x" prefix from hexadecimal
	if (base == 16 && strncasecmp(str, "0x", 2) == 0 && get_base_digit(str[2], base) != -1)
		str += 2;

	bool overflow = false;

	T result = 0;
	// calculate the value of the number in string
	while (!overflow)
	{
		int digit = get_base_digit(*str, base);
		if (digit == -1)
			break;
		str++;

		overflow = will_digit_append_overflow(result, digit, base);
		if (!overflow)
		{
			if (negative && !BAN::is_unsigned_v<T>)
				digit = -digit;
			result = result * base + digit;
		}
	}

	if (negative && BAN::is_unsigned_v<T>)
		result = -result;

	// save endp if asked
	if (endp)
	{
		while (get_base_digit(*str, base) != -1)
			str++;
		*endp = const_cast<char*>(str);
	}

	// return error on overflow
	if (overflow)
	{
		error = ERANGE;
		if constexpr(BAN::is_unsigned_v<T>)
			return BAN::numeric_limits<T>::max();
		return negative ? BAN::numeric_limits<T>::min() : BAN::numeric_limits<T>::max();
	}

	return result;
}

template<BAN::floating_point T>
static T strtoT(const char* str, char** endp, int& error)
{
	// find nan end including possible n-char-sequence
	auto get_nan_end = [](const char* str) -> const char*
	{
		ASSERT(strcasecmp(str, "nan") == 0);
		if (str[3] != '(')
			return str + 3;
		for (size_t i = 4; isalnum(str[i]) || str[i] == '_'; i++)
			if (str[i] == ')')
				return str + i + 1;
		return str + 3;
	};

	// skip whitespace
	while (isspace(*str))
		str++;

	// get sign and skip it
	bool negative = (*str == '-');
	if (*str == '-' || *str == '+')
		str++;

	// check for infinity or nan
	{
		T result = 0;

		if (strncasecmp(str, "inf", 3) == 0)
		{
			result = BAN::numeric_limits<T>::infinity();
			str += strncasecmp(str, "infinity", 8) ? 3 : 8;
		}
		else if (strncasecmp(str, "nan", 3) == 0)
		{
			result = BAN::numeric_limits<T>::quiet_NaN();
			str = get_nan_end(str);
		}

		if (result != 0)
		{
			if (endp)
				*endp = const_cast<char*>(str);
			return negative ? -result : result;
		}
	}

	// no conversion can be performed -- not ([digit] || .[digit])
	if (!(isdigit(*str) || (str[0] == '.' && isdigit(str[1]))))
	{
		error = EINVAL;
		return 0;
	}

	int base = 10;
	int exponent = 0;
	int exponents_per_digit = 1;

	// check whether we have base 16 value -- (0x[xdigit] || 0x.[xdigit])
	if (strncasecmp(str, "0x", 2) == 0 && (isxdigit(str[2]) || (str[2] == '.' && isxdigit(str[3]))))
	{
		base = 16;
		exponents_per_digit = 4;
		str += 2;
	}

	// parse whole part
	T result = 0;
	T multiplier = 1;
	while (true)
	{
		int digit = get_base_digit(*str, base);
		if (digit == -1)
			break;
		str++;

		if (result)
			exponent += exponents_per_digit;
		if (digit)
			result += multiplier * digit;
		if (result)
			multiplier /= base;
	}

	if (*str == '.')
		str++;

	while (true)
	{
		int digit = get_base_digit(*str, base);
		if (digit == -1)
			break;
		str++;

		if (result == 0)
			exponent -= exponents_per_digit;
		if (digit)
			result += multiplier * digit;
		if (result)
			multiplier /= base;
	}

	if (tolower(*str) == (base == 10 ? 'e' : 'p'))
	{
		char* maybe_end = nullptr;
		int exp_error = 0;

		int extra_exponent = strtoT<int>(str + 1, &maybe_end, 10, exp_error);
		if (exp_error != EINVAL)
		{
			if (exp_error == ERANGE || BAN::Math::will_addition_overflow(exponent, extra_exponent))
				exponent = negative ? BAN::numeric_limits<int>::min() : BAN::numeric_limits<int>::max();
			else
				exponent += extra_exponent;
			str = maybe_end;
		}
	}

	if (endp)
		*endp = const_cast<char*>(str);

	// no over/underflow can happed with zero
	if (result == 0)
		return 0;

	const int max_exponent = (base == 10) ? BAN::numeric_limits<T>::max_exponent10() : BAN::numeric_limits<T>::max_exponent2();
	if (exponent > max_exponent)
	{
		error = ERANGE;
		result = BAN::numeric_limits<T>::infinity();
		return negative ? -result : result;
	}

	const int min_exponent = (base == 10) ? BAN::numeric_limits<T>::min_exponent10() : BAN::numeric_limits<T>::min_exponent2();
	if (exponent < min_exponent)
	{
		error = ERANGE;
		result = 0;
		return negative ? -result : result;
	}

	if (exponent)
		result *= BAN::Math::pow<T>((base == 10) ? 10 : 2, exponent);
	return negative ? -result : result;
}

double atof(const char* str)
{
	return strtod(str, nullptr);
}

int atoi(const char* str)
{
	return strtol(str, nullptr, 10);
}

long atol(const char* str)
{
	return strtol(str, nullptr, 10);
}

long long atoll(const char* str)
{
	return strtoll(str, nullptr, 10);
}

float strtof(const char* __restrict str, char** __restrict endp)
{
	return strtoT<float>(str, endp, errno);
}

double strtod(const char* __restrict str, char** __restrict endp)
{
	return strtoT<double>(str, endp, errno);
}

long double strtold(const char* __restrict str, char** __restrict endp)
{
	return strtoT<long double>(str, endp, errno);
}

long strtol(const char* __restrict str, char** __restrict endp, int base)
{
	return strtoT<long>(str, endp, base, errno);
}

long long strtoll(const char* __restrict str, char** __restrict endp, int base)
{
	return strtoT<long long>(str, endp, base, errno);
}

unsigned long strtoul(const char* __restrict str, char** __restrict endp, int base)
{
	return strtoT<unsigned long>(str, endp, base, errno);
}

unsigned long long strtoull(const char* __restrict str, char** __restrict endp, int base)
{
	return strtoT<unsigned long long>(str, endp, base, errno);
}

char* realpath(const char* __restrict file_name, char* __restrict resolved_name)
{
	char buffer[PATH_MAX] {};
	long canonical_length = syscall(SYS_REALPATH, file_name, buffer);
	if (canonical_length == -1)
		return NULL;
	if (resolved_name == NULL)
	{
		resolved_name = static_cast<char*>(malloc(canonical_length + 1));
		if (resolved_name == NULL)
			return NULL;
	}
	strcpy(resolved_name, buffer);
	return resolved_name;
}

int system(const char* command)
{
	// FIXME: maybe implement POSIX compliant shell?
	constexpr const char* shell_path = "/bin/Shell";

	if (command == nullptr)
	{
		struct stat st;
		if (stat(shell_path, &st) == -1)
			return 0;
		if (S_ISDIR(st.st_mode))
			return 0;
		return !!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
	}

	struct sigaction sa;
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);

	struct sigaction sigint_save, sigquit_save;
	sigaction(SIGINT, &sa, &sigint_save);
	sigaction(SIGQUIT, &sa, &sigquit_save);

	sigset_t sigchld_save;
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &sigchld_save);

	int pid = fork();
	if (pid == 0)
	{
		sigaction(SIGINT, &sigint_save, nullptr);
		sigaction(SIGQUIT, &sigquit_save, nullptr);
		sigprocmask(SIG_SETMASK, &sigchld_save, nullptr);
		execl(shell_path, "sh", "-c", command, nullptr);
		exit(127);
	}

	int stat_val = -1;
	if (pid != -1)
	{
		while (waitpid(pid, &stat_val, 0) == -1)
		{
			if (errno == EINTR)
				continue;
			stat_val = -1;
			break;
		}
	}

	sigaction(SIGINT, &sigint_save, nullptr);
	sigaction(SIGQUIT, &sigquit_save, nullptr);
	sigprocmask(SIG_SETMASK, &sigchld_save, nullptr);

	return stat_val;
}

static size_t temp_template_count_x(const char* _template)
{
	const size_t len = strlen(_template);
	for (size_t i = 0; i < len; i++)
		if (_template[len - i - 1] != 'X')
			return i;
	return len;
}

static void generate_temp_template(char* _template, size_t x_count)
{
	const size_t len = strlen(_template);
	for (size_t i = 0; i < x_count; i++)
	{
		const uint8_t nibble = rand() & 0xF;
		_template[len - i - 1] = (nibble < 10)
			? ('0' + nibble)
			: ('a' + nibble - 10);
	}
}

char* mktemp(char* _template)
{
	const size_t x_count = temp_template_count_x(_template);
	if (x_count < 6)
	{
		errno = EINVAL;
		_template[0] = '\0';
		return _template;
	}

	for (;;)
	{
		generate_temp_template(_template, x_count);

		struct stat st;
		if (stat(_template, &st) == 0)
			return _template;
	}
}

char* mkdtemp(char* _template)
{
	const size_t x_count = temp_template_count_x(_template);
	if (x_count < 6)
	{
		errno = EINVAL;
		return nullptr;
	}

	for (;;)
	{
		generate_temp_template(_template, x_count);

		if (mkdir(_template, S_IRUSR | S_IWUSR | S_IXUSR) != -1)
			return _template;
		if (errno != EEXIST)
			return nullptr;
	}
}

int mkstemp(char* _template)
{
	const size_t x_count = temp_template_count_x(_template);
	if (x_count < 6)
	{
		errno = EINVAL;
		return -1;
	}

	for (;;)
	{
		generate_temp_template(_template, x_count);

		int fd = open(_template, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd != -1)
			return fd;
		if (errno != EEXIST)
			return -1;
	}
}

int posix_openpt(int oflag)
{
	return syscall(SYS_POSIX_OPENPT, oflag);
}

int grantpt(int)
{
	// currently posix_openpt() does this
	return 0;
}

int unlockpt(int)
{
	// currently posix_openpt() does this
	return 0;
}

char* ptsname(int fildes)
{
	static char buffer[PATH_MAX];
	if (syscall(SYS_PTSNAME, fildes, buffer, sizeof(buffer)) == -1)
		return nullptr;
	return buffer;
}

int mblen(const char* s, size_t n)
{
	if (s == nullptr)
		return 0;
	if (n == 0)
		return -1;
	switch (__getlocale(LC_CTYPE))
	{
		case LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case LOCALE_POSIX:
			return 1;
		case LOCALE_UTF8:
			const auto bytes = BAN::UTF8::byte_length(*s);
			if (bytes == BAN::UTF8::invalid)
				return -1;
			if (n < bytes)
				return -1;
			return bytes;
	}
	ASSERT_NOT_REACHED();
}

size_t mbstowcs(wchar_t* __restrict pwcs, const char* __restrict s, size_t n)
{
	size_t written = 0;

	switch (__getlocale(LC_CTYPE))
	{
		case LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case LOCALE_POSIX:
			if (pwcs == nullptr)
				written = strlen(s);
			else for (; s[written] && written < n; written++)
				pwcs[written] = s[written];
			break;
		case LOCALE_UTF8:
			const auto* us = reinterpret_cast<const unsigned char*>(s);
			for (; *us && (pwcs == nullptr || written < n); written++)
			{
				auto wch = BAN::UTF8::to_codepoint(us);
				if (wch == BAN::UTF8::invalid)
				{
					errno = EILSEQ;
					return -1;
				}
				if (pwcs != nullptr)
					pwcs[written] = wch;
				us += BAN::UTF8::byte_length(*us);
			}
			break;
	}

	if (pwcs != nullptr && written < n)
		pwcs[written] = L'\0';
	return written;
}

size_t wcstombs(char* __restrict s, const wchar_t* __restrict pwcs, size_t n)
{
	size_t written = 0;

	switch (__getlocale(LC_CTYPE))
	{
		case locale_t::LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case locale_t::LOCALE_POSIX:
			for (size_t i = 0; pwcs[i] && (s == nullptr || written < n); i++)
			{
				if (pwcs[i] > 0xFF)
					return -1;
				if (s != nullptr)
					s[written] = pwcs[i];
				written++;
			}
			break;
		case locale_t::LOCALE_UTF8:
			for (size_t i = 0; pwcs[i] && (s == nullptr || written < n); i++)
			{
				char buffer[5];
				if (!BAN::UTF8::from_codepoints(pwcs + i, 1, buffer))
					return -1;

				const size_t len = strlen(buffer);
				if (written + len > n)
					return len;

				if (s != nullptr)
					memcpy(s + written, buffer, len);
				written += len;
			}
			break;
	}

	if (s && written < n)
		s[written] = '\0';
	return written;
}

void* bsearch(const void* key, const void* base, size_t nel, size_t width, int (*compar)(const void*, const void*))
{
	if (nel == 0)
		return nullptr;

	const uint8_t* base_u8 = static_cast<const uint8_t*>(base);

	size_t l = 0;
	size_t r = nel - 1;
	while (l < r)
	{
		const size_t mid = l + (r - l) / 2;

		int res = compar(key, base_u8 + mid * width);
		if (res == 0)
			return const_cast<uint8_t*>(base_u8 + mid * width);

		if (res > 0)
			l = mid + 1;
		else
			r = mid ? mid - 1 : 0;
	}

	if (l < nel && compar(key, base_u8 + l * width) == 0)
		return const_cast<uint8_t*>(base_u8 + l * width);
	return nullptr;
}

static void qsort_swap(void* lhs, void* rhs, size_t width)
{
	uint8_t* ulhs = static_cast<uint8_t*>(lhs);
	uint8_t* urhs = static_cast<uint8_t*>(rhs);

	uint8_t buffer[64];
	while (width > 0)
	{
		const size_t to_swap = BAN::Math::min(width, sizeof(buffer));
		memcpy(buffer, ulhs, to_swap);
		memcpy(ulhs, urhs, to_swap);
		memcpy(urhs, buffer, to_swap);
		width -= to_swap;
		ulhs += to_swap;
		urhs += to_swap;
	}
}

static uint8_t* qsort_partition(uint8_t* pbegin, uint8_t* pend, size_t width, int (*compar)(const void*, const void*))
{
	uint8_t* pivot = pend - width;
	uint8_t* p1 = pbegin;
	for (uint8_t* p2 = pbegin; p2 < pivot; p2 += width)
	{
		if (compar(p2, pivot) >= 0)
			continue;
		qsort_swap(p1, p2, width);
		p1 += width;
	}
	qsort_swap(p1, pivot, width);
	return p1;
}

static void qsort_impl(uint8_t* pbegin, uint8_t* pend, size_t width, int (*compar)(const void*, const void*))
{
	if ((pend - pbegin) / width <= 1)
		return;
	uint8_t* mid = qsort_partition(pbegin, pend, width, compar);
	qsort_impl(pbegin, mid, width, compar);
	qsort_impl(mid + width, pend, width, compar);
}

void qsort(void* base, size_t nel, size_t width, int (*compar)(const void*, const void*))
{
	if (width == 0)
		return;
	uint8_t* pbegin = static_cast<uint8_t*>(base);
	qsort_impl(pbegin, pbegin + nel * width, width, compar);
}

// Constants and algorithm from https://en.wikipedia.org/wiki/Permuted_congruential_generator

static uint64_t s_rand_state = 0x4d595df4d0f33173;
static constexpr uint64_t s_rand_multiplier = 6364136223846793005;
static constexpr uint64_t s_rand_increment = 1442695040888963407;

static constexpr uint32_t rotr32(uint32_t x, unsigned r)
{
	return x >> r | x << (-r & 31);
}

int rand(void)
{
	uint64_t x = s_rand_state;
	unsigned count = (unsigned)(x >> 59);

	s_rand_state = x * s_rand_multiplier + s_rand_increment;
	x ^= x >> 18;

	return rotr32(x >> 27, count) % RAND_MAX;
}

void srand(unsigned int seed)
{
	s_rand_state = seed + s_rand_increment;
	(void)rand();
}
