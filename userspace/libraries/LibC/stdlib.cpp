#include <BAN/Assert.h>
#include <BAN/Limits.h>
#include <BAN/Math.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <icxxabi.h>

extern "C" char** environ;

extern "C" void _fini();

static void (*at_exit_funcs[64])();
static uint32_t at_exit_funcs_count = 0;

void abort(void)
{
	fflush(nullptr);
	fprintf(stderr, "abort()\n");
	exit(1);
}

void exit(int status)
{
	for (uint32_t i = at_exit_funcs_count; i > 0; i--)
		at_exit_funcs[i - 1]();
	fflush(nullptr);
	__cxa_finalize(nullptr);
	_fini();
	_exit(status);
	ASSERT_NOT_REACHED();
}

int abs(int val)
{
	return val < 0 ? -val : val;
}

int atexit(void (*func)(void))
{
	if (at_exit_funcs_count > sizeof(at_exit_funcs) / sizeof(*at_exit_funcs))
	{
		errno = ENOBUFS;
		return -1;
	}
	at_exit_funcs[at_exit_funcs_count++] = func;
	return 0;
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
static constexpr bool will_digit_append_overflow(bool negative, T current, int digit, int base)
{
	if (BAN::is_unsigned_v<T> && negative && digit)
		return true;
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

		overflow = will_digit_append_overflow(negative, result, digit, base);
		if (!overflow)
			result = result * base + (negative ? -digit : digit);
	}

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
	return result;
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

char* getenv(const char* name)
{
	if (environ == nullptr)
		return nullptr;
	size_t len = strlen(name);
	for (int i = 0; environ[i]; i++)
		if (strncmp(name, environ[i], len) == 0)
			if (environ[i][len] == '=')
				return environ[i] + len + 1;
	return nullptr;
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
	// FIXME
	if (command == nullptr)
		return 1;

	int pid = fork();
	if (pid == 0)
	{
		execl("/bin/Shell", "Shell", "-c", command, (char*)0);
		exit(1);
	}

	if (pid == -1)
		return -1;

	int stat_val;
	waitpid(pid, &stat_val, 0);
	return stat_val;
}

int setenv(const char* name, const char* val, int overwrite)
{
	if (name == nullptr || !name[0] || strchr(name, '='))
	{
		errno = EINVAL;
		return -1;
	}

	if (!overwrite && getenv(name))
		return 0;

	size_t namelen = strlen(name);
	size_t vallen = strlen(val);

	char* string = (char*)malloc(namelen + vallen + 2);
	memcpy(string, name, namelen);
	string[namelen] = '=';
	memcpy(string + namelen + 1, val, vallen);
	string[namelen + vallen + 1] = '\0';

	return putenv(string);
}

int unsetenv(const char* name)
{
	if (name == nullptr || !name[0] || strchr(name, '='))
	{
		errno = EINVAL;
		return -1;
	}

	size_t len = strlen(name);

	bool found = false;
	for (int i = 0; environ[i]; i++)
	{
		if (!found && strncmp(environ[i], name, len) == 0 && environ[i][len] == '=')
		{
			free(environ[i]);
			found = true;
		}
		if (found)
			environ[i] = environ[i + 1];
	}

	return 0;
}

int putenv(char* string)
{
	if (string == nullptr || !string[0])
	{
		errno = EINVAL;
		return -1;
	}

	if (!environ)
	{
		environ = (char**)malloc(sizeof(char*) * 2);
		if (!environ)
			return -1;
		environ[0] = string;
		environ[1] = nullptr;
		return 0;
	}

	int cnt = 0;
	for (int i = 0; string[i]; i++)
		if (string[i] == '=')
			cnt++;
	if (cnt != 1)
	{
		errno = EINVAL;
		return -1;
	}

	int namelen = strchr(string, '=') - string;
	for (int i = 0; environ[i]; i++)
	{
		if (strncmp(environ[i], string, namelen + 1) == 0)
		{
			free(environ[i]);
			environ[i] = string;
			return 0;
		}
	}

	int env_count = 0;
	while (environ[env_count])
		env_count++;

	char** new_envp = (char**)malloc(sizeof(char*) * (env_count + 2));
	if (new_envp == nullptr)
		return -1;

	for (int i = 0; i < env_count; i++)
		new_envp[i] = environ[i];
	new_envp[env_count] = string;
	new_envp[env_count + 1] = nullptr;

	free(environ);
	environ = new_envp;

	return 0;
}

void* bsearch(const void* key, const void* base, size_t nel, size_t width, int (*compar)(const void*, const void*))
{
	if (nel == 0)
		return nullptr;

	const uint8_t* base_u8 = reinterpret_cast<const uint8_t*>(base);

	size_t l = 0;
	size_t r = nel - 1;
	while (l <= r)
	{
		const size_t mid = (l + r) / 2;

		int res = compar(key, base_u8 + mid * width);
		if (res == 0)
			return const_cast<uint8_t*>(base_u8 + mid * width);

		if (res < 0)
			r = mid - 1;
		else
			l = mid + 1;
	}

	return nullptr;
}

static void qsort_swap(void* lhs, void* rhs, size_t width)
{
	uint8_t buffer[64];
	size_t swapped = 0;
	while (swapped < width)
	{
		const size_t to_swap = BAN::Math::min(width - swapped, sizeof(buffer));
		memcpy(buffer, lhs, to_swap);
		memcpy(lhs, rhs, to_swap);
		memcpy(rhs, buffer, to_swap);
		swapped += to_swap;
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
