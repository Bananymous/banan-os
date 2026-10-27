#include <BAN/Math.h>
#include <BAN/Move.h>

#include <dlfcn.h>
#include <execinfo.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>

int backtrace(void* buffer[], int size)
{
	struct stackframe
	{
		const stackframe* bp;
		void* ip;
	};

	const auto* stackframe = static_cast<struct stackframe*>(__builtin_frame_address(0));

	int i = 0;
	for (; i < size && stackframe; i++, stackframe = stackframe->bp)
		buffer[i] = stackframe->ip;

	return i;
}

template<typename F>
static void get_symbol_info(const void* address, const F& callback)
{
	const uintptr_t uptr_addr = reinterpret_cast<uintptr_t>(address);

	Dl_info_t dli;
	if (dladdr(address, &dli) == 0)
		return callback("0x%08" PRIxPTR "\n", uptr_addr);

	const uintptr_t uptr_fbase = reinterpret_cast<uintptr_t>(dli.dli_fbase);
	if (dli.dli_saddr == nullptr || dli.dli_sname == nullptr)
		return callback("0x%08" PRIxPTR " (%s)\n", uptr_addr - uptr_fbase, dli.dli_fname);

	const uintptr_t uptr_saddr = reinterpret_cast<uintptr_t>(dli.dli_saddr);
	return callback("0x%08" PRIxPTR " (%s) %s+0x%" PRIxPTR "\n", uptr_addr - uptr_fbase, dli.dli_fname, dli.dli_sname, uptr_addr - uptr_saddr);
}

char** backtrace_symbols(void* const buffer[], int size)
{
	size_t storage_cap = size * (sizeof(char*) + 16);
	size_t storage_len = size *  sizeof(char*);

	void* storage = malloc(storage_cap);
	if (storage == nullptr)
		return nullptr;

	for (int i = 0; i < size; i++)
	{
		bool success { false };
		get_symbol_info(buffer[i], [&]<typename... Args>(const char* format, Args&&... args) {
			char* base = static_cast<char*>(storage) + storage_len;
			const int length = snprintf(base, storage_cap - storage_len, format, BAN::forward<Args>(args)...);

			if (storage_len + length > storage_cap)
			{
				const size_t new_storage_cap = BAN::Math::max(storage_cap + length, storage_cap * 2);

				void* new_storage = realloc(storage, new_storage_cap);
				if (new_storage == nullptr)
					return;
				storage_cap = new_storage_cap;
				storage     = new_storage;

				base = static_cast<char*>(storage) + storage_len;
				snprintf(base, storage_cap - storage_len, format, BAN::forward<Args>(args)...);
			}

			base[length] = '\0';
			static_cast<char**>(storage)[i] = base;
			storage_len += length;

			success = true;
		});

		if (!success)
		{
			free(storage);
			return nullptr;
		}
	}

	return static_cast<char**>(storage);
}

void backtrace_symbols_fd(void* const buffer[], int size, int fd)
{
	for (int i = 0; i < size; i++)
	{
		get_symbol_info(buffer[i], [&]<typename... Args>(const char* format, Args&&... args) {
			dprintf(fd, format, BAN::forward<Args>(args)...);
		});
	}
}
