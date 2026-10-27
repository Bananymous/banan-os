#include <BAN/Assert.h>
#include <BAN/Limits.h>
#include <BAN/Math.h>
#include <BAN/UTF8.h>

#include <errno.h>
#include <execinfo.h>
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

#include <bits/strtoT.hpp>

#include <icxxabi.h>

void abort(void)
{
	if (const int fd = open("/dev/debug", O_RDONLY); fd != -1)
	{
		void* buffer[128];
		if (const int count = backtrace(buffer, 128); count > 0)
			backtrace_symbols_fd(buffer, count, fd);
		close(fd);
	}

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
}

void _Exit(int status)
{
	_exit(status);
}

int atexit(void (*func)(void))
{
	void* func_addr = reinterpret_cast<void*>(func);
	return __cxa_atexit([](void* func_ptr) { reinterpret_cast<void (*)(void)>(func_ptr)(); }, func_addr, nullptr);
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

int abs(int val)
{
	return val < 0 ? -val : val;
}

long labs(long val)
{
	return val < 0 ? -val : val;
}

long long llabs(long long val)
{
	return val < 0 ? -val : val;
}

div_t div(int numer, int denom)
{
	return {
		.quot = numer / denom,
		.rem = numer % denom,
	};
}

ldiv_t ldiv(long numer, long denom)
{
	return {
		.quot = numer / denom,
		.rem = numer % denom,
	};
}

lldiv_t lldiv(long long numer, long long denom)
{
	return {
		.quot = numer / denom,
		.rem = numer % denom,
	};
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

static void randomize_temp(char* buffer)
{
	// FIXME: don't use rand()
	const uint32_t value = rand() & 0xFFFFFF;
	sprintf(buffer, "%06x", value);
}

static char* validate_temp_template(char* _template, int suffixlen)
{
	const size_t length = strlen(_template);
	if (suffixlen < 0 || length < static_cast<size_t>(suffixlen + 6))
	{
		errno = EINVAL;
		return nullptr;
	}

	char* xptr = _template + length - suffixlen - 6;
	if (memcmp(xptr, "XXXXXX", 6) != 0)
	{
		errno = EINVAL;
		return nullptr;
	}

	return xptr;
}

char* mktemp(char* _template)
{
	char* xptr = validate_temp_template(_template, 0);
	if (xptr == nullptr)
	{
		errno = EINVAL;
		_template[0] = '\0';
		return _template;
	}

	for (;;)
	{
		randomize_temp(xptr);

		struct stat st;
		if (stat(_template, &st) == -1)
		{
			if (errno != ENOENT)
				_template[0] = '\0';
			return _template;
		}
	}
}

char* mkdtemp(char* _template)
{
	char* xptr = validate_temp_template(_template, 0);
	if (xptr == nullptr)
	{
		errno = EINVAL;
		return nullptr;
	}

	for (;;)
	{
		randomize_temp(xptr);
		if (mkdir(_template, S_IRUSR | S_IWUSR | S_IXUSR) != -1)
			return _template;
		if (errno != EEXIST)
			return nullptr;
	}
}

int mkstemp(char* _template)
{
	return mkostemps(_template, 0, 0);
}

int mkostemp(char* _template, int flags)
{
	return mkostemps(_template, 0, flags);
}

int mkstemps(char* _template, int suffixlen)
{
	return mkostemps(_template, suffixlen, 0);
}

int mkostemps(char* _template, int suffixlen, int flags)
{
	flags &= O_APPEND | O_CLOEXEC | O_SYNC;
	flags |= O_RDWR | O_CREAT | O_EXCL;

	char* xptr = validate_temp_template(_template, suffixlen);
	if (xptr == nullptr)
	{
		errno = EINVAL;
		return -1;
	}

	for (;;)
	{
		randomize_temp(xptr);
		if (int fd = open(_template, flags, 0600); fd != -1)
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

	switch (__getlocale(LC_CTYPE)->encoding)
	{
		case __ENC_ASCII:
			return 1;
		case __ENC_UTF8:
			const auto bytes = BAN::UTF8::byte_length(*s);
			if (bytes == BAN::UTF8::invalid)
				return -1;
			if (n < bytes)
				return -1;
			return bytes;
	}

	ASSERT_NOT_REACHED();
}

int mbtowc(wchar_t* __restrict pwc, const char* __restrict s, size_t n)
{
	// no state-dependent encodings
	if (s == nullptr)
		return 0;

	switch (__getlocale(LC_CTYPE)->encoding)
	{
		case __ENC_ASCII:
			if (pwc != nullptr)
				*pwc = *s;
			return *s ? 1 : 0;
		case __ENC_UTF8:
			const auto* us = reinterpret_cast<const unsigned char*>(s);

			const uint32_t length = BAN::UTF8::byte_length(*us);
			if (length == BAN::UTF8::invalid || n < length)
			{
				errno = EILSEQ;
				return -1;
			}

			const auto wch = BAN::UTF8::to_codepoint(us);
			if (wch == BAN::UTF8::invalid)
			{
				errno = EILSEQ;
				return -1;
			}

			if (pwc)
				*pwc = wch;

			return wch ? length : 0;
	}

	ASSERT_NOT_REACHED();
}

size_t mbstowcs(wchar_t* __restrict pwcs, const char* __restrict s, size_t n)
{
	size_t written = 0;

	switch (__getlocale(LC_CTYPE)->encoding)
	{
		case __ENC_ASCII:
			if (pwcs == nullptr)
				written = strlen(s);
			else for (; s[written] && written < n; written++)
				pwcs[written] = s[written];
			break;
		case __ENC_UTF8:
		{
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
		default:
			ASSERT_NOT_REACHED();
	}

	if (pwcs != nullptr && written < n)
		pwcs[written] = L'\0';
	return written;
}

int wctomb(char* s, wchar_t wchar)
{
	// no state-dependent encodings
	if (s == nullptr)
		return 0;

	switch (__getlocale(LC_CTYPE)->encoding)
	{
		case __ENC_ASCII:
			*s = wchar;
			return wchar ? 1 : 0;
		case __ENC_UTF8:
			char buffer[5];
			if (!BAN::UTF8::from_codepoints(&wchar, 1, buffer))
				return -1;
			const size_t length = strlen(buffer);
			memcpy(s, buffer, length);
			return length;
	}

	ASSERT_NOT_REACHED();
}

size_t wcstombs(char* __restrict s, const wchar_t* __restrict pwcs, size_t n)
{
	size_t written = 0;

	switch (__getlocale(LC_CTYPE)->encoding)
	{
		case __ENC_ASCII:
			for (size_t i = 0; pwcs[i] && (s == nullptr || written < n); i++)
			{
				if (pwcs[i] > 0xFF)
					return -1;
				if (s != nullptr)
					s[written] = pwcs[i];
				written++;
			}
			break;
		case __ENC_UTF8:
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
		default:
			ASSERT_NOT_REACHED();
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

template<typename T>
static inline void qsort_swap_fixed(void* lhs, void* rhs)
{
	T temp;
	memcpy(&temp, lhs, sizeof(T));
	memcpy(lhs,   rhs, sizeof(T));
	memcpy(rhs, &temp, sizeof(T));
}

static void qsort_swap_generic(void* lhs, void* rhs, size_t width)
{
	uint8_t temp[64];

	uint8_t* ulhs = static_cast<uint8_t*>(lhs);
	uint8_t* urhs = static_cast<uint8_t*>(rhs);

	while (width >= sizeof(temp))
	{
		memcpy(temp, ulhs, sizeof(temp));
		memcpy(ulhs, urhs, sizeof(temp));
		memcpy(urhs, temp, sizeof(temp));
		width -= sizeof(temp);
		ulhs  += sizeof(temp);
		urhs  += sizeof(temp);
	}

	if (width > 0)
	{
		memcpy(temp, ulhs, width);
		memcpy(ulhs, urhs, width);
		memcpy(urhs, temp, width);
	}
}

static void qsort_swap(uint8_t* lhs, uint8_t* rhs, size_t width)
{
	switch (width)
	{
		case 1: return qsort_swap_fixed<uint8_t> (lhs, rhs);
		case 2: return qsort_swap_fixed<uint16_t>(lhs, rhs);
		case 4: return qsort_swap_fixed<uint32_t>(lhs, rhs);
		case 8: return qsort_swap_fixed<uint64_t>(lhs, rhs);
	}

	qsort_swap_generic(lhs, rhs, width);
}

static uint8_t* qsort_get_pivot(uint8_t* pbegin, uint8_t* pend, size_t width, int (*compar)(const void*, const void*, void*), void* arg)
{
	const auto median = [compar, arg](uint8_t* a, uint8_t* b, uint8_t* c) {
		if (compar(a, b, arg) < 0)
		{
			return compar(b, c, arg) < 0 ? b
			     : compar(a, c, arg) < 0 ? c
			     : a;
		}
		else
		{
			return compar(a, c, arg) < 0 ? a
			     : compar(b, c, arg) < 0 ? c
			     : b;
		}
	};

	const size_t nel = (pend - pbegin) / width;
	return median(
		median(
			pbegin + 0 * nel / 8 * width,
			pbegin + 1 * nel / 8 * width,
			pbegin + 2 * nel / 8 * width
		),
		median(
			pbegin + 3 * nel / 8 * width,
			pbegin + 4 * nel / 8 * width,
			pbegin + 5 * nel / 8 * width
		),
		median(
			pbegin + 6 * nel / 8 * width,
			pbegin + 7 * nel / 8 * width,
			pend - width
		)
	);
}

struct qsort_pair
{
	uint8_t* lt;
	uint8_t* gt;
};

static qsort_pair qsort_partition(uint8_t* pbegin, uint8_t* pend, size_t width, int (*compar)(const void*, const void*, void*), void* arg)
{
	uint8_t* pivot = pbegin;
	qsort_swap(pivot, qsort_get_pivot(pbegin, pend, width, compar, arg), width);

	uint8_t* p = pbegin;
	uint8_t* q = pend;

	uint8_t* i = p + width;
	uint8_t* j = q - width;

	while (true)
	{
		int comp1, comp2;
		while (i <= j && (comp1 = compar(i, pivot, arg)) < 0)
			i += width;
		while (i <= j && (comp2 = compar(j, pivot, arg)) > 0)
			j -= width;

		if (i > j)
			break;

		qsort_swap(i, j, width);
		if (comp2 == 0)
			qsort_swap(p += width, i, width);
		i += width;

		if (i <= j)
		{
			if (comp1 == 0)
				qsort_swap(q -= width, j, width);
			j -= width;
		}
	}

	for (; p >= pbegin; p -= width, j -= width)
		qsort_swap(p, j, width);

	for (; q < pend; q += width, i += width)
		qsort_swap(q, i, width);

	return { j + width, i };
}

static void qsort_impl(uint8_t* pbegin, uint8_t* pend, size_t width, int (*compar)(const void*, const void*, void*), void* arg)
{
	while (pbegin + width < pend)
	{
		if (static_cast<size_t>(pend - pbegin) <= 32 * width)
		{
			for (uint8_t* ptr1 = pbegin; ptr1 < pend; ptr1 += width)
				for (uint8_t* ptr2 = ptr1; ptr2 != pbegin && compar(ptr2 - width, ptr2, arg) > 0; ptr2 -= width)
					qsort_swap(ptr2 - width, ptr2, width);
			break;
		}

		auto [lt, gt] = qsort_partition(pbegin, pend, width, compar, arg);
		if (lt - pbegin < pend - gt)
		{
			qsort_impl(pbegin, lt, width, compar, arg);
			pbegin = gt;
		}
		else
		{
			qsort_impl(gt, pend, width, compar, arg);
			pend = lt;
		}
	}
}

void qsort_r(void* base, size_t nel, size_t width, int (*compar)(const void*, const void*, void*), void* arg)
{
	if (width == 0 || nel <= 1)
		return;
	uint8_t* pbegin = static_cast<uint8_t*>(base);
	qsort_impl(pbegin, pbegin + nel * width, width, compar, arg);
}

void qsort(void* base, size_t nel, size_t width, int (*compar)(const void*, const void*))
{
	struct qsort_info_t {
		int (*compar)(const void*, const void*);
	};
	constexpr auto qsort_compar = [](const void* a, const void* b, void* info) -> int {
		return static_cast<qsort_info_t*>(info)->compar(a, b);
	};
	qsort_info_t info { compar };
	return qsort_r(base, nel, width, qsort_compar, &info);
}

// Constants and algorithm from https://en.wikipedia.org/wiki/Permuted_congruential_generator

static constexpr uint64_t s_rand_multiplier = 6364136223846793005;
static constexpr uint64_t s_rand_increment = 1442695040888963407;
static uint64_t s_rand_state;

template<BAN::integral T>
static inline int rand_impl(T& state)
{
	uint64_t x;
	if constexpr (sizeof(T) == 8) x = state;
	if constexpr (sizeof(T) == 4) x = state * 0x0000000100000001;
	if constexpr (sizeof(T) == 2) x = state * 0x0001000100010001;
	if constexpr (sizeof(T) == 1) x = state * 0x0101010101010101;

	const unsigned count = x >> 59;

	state = x * s_rand_multiplier + s_rand_increment;
	x ^= x >> 18;

	constexpr auto rotr32 = [](uint32_t x, unsigned r) {
		return x >> r | x << (-r & 31);
	};

	return rotr32(x >> 27, count) % RAND_MAX;
}

int rand_r(unsigned* seed)
{
	return rand_impl(*seed);
}

int rand(void)
{
	return rand_impl(s_rand_state);
}

void srand(unsigned int seed)
{
	s_rand_state = seed + s_rand_increment;
	(void)rand();
}

struct random_state_t
{
	uint8_t type;
	uint8_t idx1;
	uint8_t idx2;
	uint32_t table[];
};

static constexpr size_t s_random_state_sizes[] { 8, 32, 64, 128, 256 };
static constexpr size_t s_random_state_size_count = sizeof(s_random_state_sizes) / sizeof(*s_random_state_sizes);

static char* s_random_state;

static size_t get_random_state_size()
{
	const auto& state = *reinterpret_cast<random_state_t*>(s_random_state);
	ASSERT(state.type < s_random_state_size_count);
	return s_random_state_sizes[state.type];
}

static size_t get_random_table_size()
{
	return (get_random_state_size() - offsetof(random_state_t, table)) / sizeof(uint32_t);
}

long random(void)
{
	auto& state = *reinterpret_cast<random_state_t*>(s_random_state);
	const size_t table_size = get_random_table_size();
	const uint32_t result = state.table[state.idx1] += state.table[state.idx2];
	state.idx1 = (state.idx1 + 1) % table_size;
	state.idx2 = (state.idx2 + 1) % table_size;
	return result >> 1;
}

void srandom(unsigned seed)
{
	initstate(seed, s_random_state, get_random_state_size());
}

char* initstate(unsigned seed, char* statebuf, size_t size)
{
	if (size < 8)
		return NULL;

	char* old_state = s_random_state;
	s_random_state = statebuf;

	auto& new_state = *reinterpret_cast<random_state_t*>(s_random_state);
	for (size_t i = 0; i < s_random_state_size_count && size >= s_random_state_sizes[i]; i++)
		new_state.type = i;

	const size_t table_size = get_random_table_size();
	new_state.idx1 = 0;
	new_state.idx2 = table_size - 1;

	uint64_t value = seed;
	for (size_t i = 0; i < table_size; i++)
		new_state.table[i] = value = (16807 * value) % 0x7FFFFFFF;

	return old_state;
}

char* setstate(char* state)
{
	char* old_state = s_random_state;
	s_random_state = state;
	return old_state;
}

__attribute__((constructor))
void init_default_random()
{
	static char buffer[128];
	initstate(1, buffer, 128);
	srand(1);
}
