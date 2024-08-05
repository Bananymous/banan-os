#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <BAN/Math.h>

#include <bits/printf.h>
#include <errno.h>
#include <fcntl.h>
#include <scanf_impl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct FILE
{
	int fd			{ -1 };
	mode_t mode		{ 0 };
	int buffer_type	{ _IOLBF };
	bool eof		{ false };
	bool error		{ false };

	int pid			{ -1 };

	unsigned char inline_buffer_storage[BUFSIZ] {};
	unsigned char* buffer = inline_buffer_storage;
	uint32_t buffer_size = BUFSIZ;
	uint32_t buffer_index { 0 };
};

struct ScopeLock
{
	ScopeLock(FILE* file)
		: m_file(file)
	{
		flockfile(m_file);
	}

	~ScopeLock()
	{
		funlockfile(m_file);
	}

	FILE* m_file;
};

static FILE s_files[FOPEN_MAX] {
	{ .fd = STDIN_FILENO,	.mode = O_RDONLY },
	{ .fd = STDOUT_FILENO,	.mode = O_WRONLY },
	{ .fd = STDERR_FILENO,	.mode = O_WRONLY, .buffer_type = _IONBF },
	{ .fd = STDDBG_FILENO,	.mode = O_WRONLY },
};

FILE* stdin  = &s_files[0];
FILE* stdout = &s_files[1];
FILE* stderr = &s_files[2];
FILE* stddbg = &s_files[3];

void clearerr(FILE* file)
{
	ScopeLock _(file);
	file->eof = false;
	file->error = false;
}

char* ctermid(char* buffer)
{
	static char s_buffer[L_ctermid];
	char* target = buffer ? buffer : s_buffer;
	syscall(SYS_TERMID, target);
	return target;
}

int fclose(FILE* file)
{
	ScopeLock _(file);
	(void)fflush(file);
	int ret = (close(file->fd) == -1) ? EOF : 0;
	file = {};
	return ret;
}

static mode_t parse_mode_string(const char* mode_str)
{
	size_t len = strlen(mode_str);
	if (len == 0 || len > 3)
		return 0;
	if (len == 3 && mode_str[1] == mode_str[2])
		return 0;
	if (strspn(mode_str + 1, "b+") != len - 1)
		return 0;
	bool plus = (mode_str[1] == '+' || mode_str[2] == '+');
	switch (mode_str[0])
	{
		case 'r': return plus ? O_RDWR : O_RDONLY;
		case 'w': return plus ? O_RDWR | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_TRUNC;
		case 'a': return plus ? O_RDWR | O_CREAT | O_APPEND : O_WRONLY | O_CREAT | O_APPEND;
	}
	return 0;
}

FILE* fdopen(int fd, const char* mode_str)
{
	mode_t mode = parse_mode_string(mode_str);
	if (mode == 0)
	{
		errno = EINVAL;
		return nullptr;
	}

	// FIXME: when threads are implemented
	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = fd;
		s_files[i].mode = mode & O_ACCMODE;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer_storage);
		ASSERT(s_files[i].buffer_size == BUFSIZ);
		return &s_files[i];
	}

	errno = EMFILE;
	return nullptr;
}

int feof(FILE* file)
{
	return file->eof;
}

int ferror(FILE* file)
{
	return file->error;
}

int fflush(FILE* file)
{
	if (file == nullptr)
	{
		for (int i = 0; i < FOPEN_MAX; i++)
			if (s_files[i].fd != -1)
				if (int ret = fflush(&s_files[i]); ret != 0)
					return ret;
		return 0;
	}

	ScopeLock _(file);

	if (file->buffer_index == 0)
		return 0;

	if (syscall(SYS_WRITE, file->fd, file->buffer, file->buffer_index) < 0)
	{
		file->error = true;
		return EOF;
	}

	file->buffer_index = 0;
	return 0;
}

int fgetc(FILE* file)
{
	ScopeLock _(file);
	return getc_unlocked(file);
}

int fgetpos(FILE* file, fpos_t* pos)
{
	ScopeLock _(file);
	off_t offset = ftello(file);
	if (offset == -1)
		return -1;
	*pos = offset;
	return 0;
}

char* fgets(char* str, int size, FILE* file)
{
	if (size == 0)
		return nullptr;
	ScopeLock _(file);
	int i = 0;
	for (; i < size - 1; i++)
	{
		int c = getc_unlocked(file);
		if (c == EOF)
		{
			if (i == 0)
				return nullptr;
			break;
		}
		str[i] = c;
		if (c == '\n')
		{
			i++;
			break;
		}
	}
	str[i] = '\0';
	return str;
}

int fileno(FILE* fp)
{
	if (fp == nullptr)
		return EBADF;
	return fp->fd;
}

void flockfile(FILE*)
{
	// FIXME: when threads are implemented
}

FILE* fopen(const char* pathname, const char* mode_str)
{
	mode_t mode = parse_mode_string(mode_str);
	if (mode == 0)
	{
		errno = EINVAL;
		return nullptr;
	}

	int fd = open(pathname, mode, 0666);
	if (fd == -1)
		return nullptr;

	// FIXME: when threads are implemented
	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = fd;
		s_files[i].mode = mode & O_ACCMODE;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer_storage);
		ASSERT(s_files[i].buffer_size == BUFSIZ);
		return &s_files[i];
	}

	errno = EMFILE;
	return nullptr;
}

int fprintf(FILE* file, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vfprintf(file, format, arguments);
	va_end(arguments);
	return ret;
}

int fputc(int c, FILE* file)
{
	ScopeLock _(file);
	return putc_unlocked(c, file);
}

int fputs(const char* str, FILE* file)
{
	ScopeLock _(file);
	while (*str)
	{
		if (putc_unlocked(*str, file) == EOF)
			return EOF;
		str++;
	}
	return 0;
}

size_t fread(void* buffer, size_t size, size_t nitems, FILE* file)
{
	ScopeLock _(file);
	if (file->eof || nitems * size == 0)
		return 0;

	size_t target = size * nitems;
	size_t nread = 0;

	while (nread < target)
	{
		ssize_t ret = syscall(SYS_READ, file->fd, (uint8_t*)buffer + nread, target - nread);

		if (ret < 0)
			file->error = true;
		else if (ret == 0)
			file->eof = true;

		if (ret <= 0)
			return nread;

		nread += ret;
	}

	return nread / size;
}

FILE* freopen(const char* pathname, const char* mode_str, FILE* file)
{
	mode_t mode = parse_mode_string(mode_str);
	if (mode == 0)
	{
		errno = EINVAL;
		return nullptr;
	}

	ScopeLock _(file);

	if (pathname)
	{
		fclose(file);
		file->fd = open(pathname, mode, 0666);
		file->mode = mode & O_ACCMODE;
		if (file->fd == -1)
			return nullptr;
	}
	else
	{
		if ((file->mode & mode) != mode)
		{
			fclose(file);
			errno = EBADF;
			return nullptr;
		}
		file->mode = mode & O_ACCMODE;
	}

	return file;
}

int fscanf(FILE* file, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vfscanf(file, format, arguments);
	va_end(arguments);
	return ret;
}

int fseek(FILE* file, long offset, int whence)
{
	return fseeko(file, offset, whence);
}

int fseeko(FILE* file, off_t offset, int whence)
{
	ScopeLock _(file);
	long ret = syscall(SYS_SEEK, file->fd, offset, whence);
	if (ret < 0)
		return -1;
	file->eof = false;
	return 0;
}

int fsetpos(FILE* file, const fpos_t* pos)
{
	return fseek(file, *pos, SEEK_SET);
}

long ftell(FILE* file)
{
	return ftello(file);
}

off_t ftello(FILE* file)
{
	ScopeLock _(file);
	long ret = syscall(SYS_TELL, file->fd);
	if (ret < 0)
		return -1;
	return ret;
}

int ftrylockfile(FILE*)
{
	// FIXME: when threads are implemented
	return 0;
}

void funlockfile(FILE*)
{
	// FIXME: when threads are implemented
}

size_t fwrite(const void* buffer, size_t size, size_t nitems, FILE* file)
{
	ScopeLock _(file);
	unsigned char* ubuffer = (unsigned char*)buffer;
	for (size_t byte = 0; byte < nitems * size; byte++)
		if (putc_unlocked(ubuffer[byte], file) == EOF)
			return byte / size;
	return nitems;
}

int getc(FILE* file)
{
	ScopeLock _(file);
	return getc_unlocked(file);
}

int getchar(void)
{
	return getc(stdin);
}

int getc_unlocked(FILE* file)
{
	if (file->eof)
		return EOF;

	unsigned char c;
	long ret = syscall(SYS_READ, file->fd, &c, 1);

	if (ret < 0)
	{
		file->error = true;
		return EOF;
	}

	if (ret == 0)
	{
		file->eof = true;
		return EOF;
	}

	return c;
}

int getchar_unlocked(void)
{
	return getc_unlocked(stdin);
}

char* gets(char* buffer)
{
	if (stdin->eof)
		return nullptr;

	unsigned char* ubuffer = (unsigned char*)buffer;

	int first = fgetc(stdin);
	if (first == EOF)
		return nullptr;
	*ubuffer++ = first;

	for (;;)
	{
		int c = fgetc(stdin);
		if (c == '\n' || c == EOF)
		{
			*ubuffer++ = '\0';
			return buffer;
		}
		*ubuffer++ = c;
	}
}

int pclose(FILE* file)
{
	if (file->pid == -1)
	{
		errno = EBADF;
		return -1;
	}

	pid_t pid = file->pid;
	(void)fclose(file);

	int stat;
	while (waitpid(pid, &stat, 0) != -1)
	{
		if (errno != EINTR)
		{
			stat = -1;
			break;
		}
	}
	return stat;
}

void perror(const char* string)
{
	ScopeLock _(stderr);
	if (string && *string)
	{
		fputs(string, stderr);
		fputs(": ", stderr);
	}
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
	stderr->error = true;
}

FILE* popen(const char* command, const char* mode_str)
{
	if ((mode_str[0] != 'r' && mode_str[0] != 'w') || mode_str[1] != '\0')
	{
		errno = EINVAL;
		return nullptr;
	}

	bool read = (mode_str[0] == 'r');

	int fds[2];
	if (pipe(fds) == -1)
		return nullptr;

	pid_t pid = fork();
	if (pid == 0)
	{
		if (read)
			dup2(fds[1], STDOUT_FILENO);
		else
			dup2(fds[0], STDIN_FILENO);
		close(fds[0]);
		close(fds[1]);

		execl("/bin/Shell", "sh", "-c", command, nullptr);
		exit(1);
	}

	if (pid == -1)
	{
		close(fds[0]);
		close(fds[1]);
		return nullptr;
	}

	close(read ? fds[1] : fds[0]);

	// FIXME: when threads are implemented
	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = read ? fds[0] : fds[1];
		s_files[i].mode = (unsigned)(read ? O_RDONLY : O_WRONLY);
		s_files[i].pid = pid;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer_storage);
		ASSERT(s_files[i].buffer_size == BUFSIZ);
		return &s_files[i];
	}

	errno = EMFILE;
	return nullptr;
}

int printf(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vfprintf(stdout, format, arguments);
	va_end(arguments);
	return ret;
}

int putc(int c, FILE* file)
{
	return fputc(c, file);
}

int putchar(int c)
{
	return putc(c, stdout);
}

int putc_unlocked(int c, FILE* file)
{
	file->buffer[file->buffer_index++] = c;
	if (file->buffer_type == _IONBF || (file->buffer_type == _IOLBF && c == '\n') || file->buffer_index >= file->buffer_size)
		if (fflush(file) == EOF)
			return EOF;
	return (unsigned char)c;
}

int putchar_unlocked(int c)
{
	return putc_unlocked(c, stdout);
}

int puts(const char* string)
{
	ScopeLock _(stdout);
	if (fputs(string, stdout) == EOF)
		return EOF;
	if (fputc('\n', stdout) == EOF)
		return EOF;
	return 0;
}

int remove(const char* path)
{
	struct stat st;
	if (stat(path, &st) == -1)
		return -1;
	if (S_ISDIR(st.st_mode))
		return rmdir(path);
	return unlink(path);
}

int rename(const char* old, const char* _new)
{
	struct stat st;
	if (lstat(old, &st) == -1)
		return -1;

	if (!S_ISREG(st.st_mode))
	{
		errno = ENOTSUP;
		return -1;
	}

	if (unlink(_new) == -1 && errno != ENOENT)
		return -1;

	int old_fd = open(old, O_RDWR);
	int new_fd = open(_new, O_RDWR | O_CREAT | O_EXCL, st.st_mode);
	if (old_fd == -1 || new_fd == -1)
		goto error;

	for (;;)
	{
		char buffer[512];
		ssize_t nread = read(old_fd, buffer, sizeof(buffer));
		if (nread == -1)
		{
			unlink(_new);
			goto error;
		}
		if (nread == 0)
			break;

		if (write(new_fd, buffer, nread) != nread)
		{
			unlink(_new);
			goto error;
		}
	}

	unlink(old);

	return 0;

error:
	if (old_fd != -1)
		close(old_fd);
	if (new_fd != -1)
		close(new_fd);
	return -1;
}

void rewind(FILE* file)
{
	ScopeLock _(file);
	file->error = false;
	(void)fseek(file, 0L, SEEK_SET);
}

int scanf(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vscanf(format, arguments);
	va_end(arguments);
	return ret;
}

void setbuf(FILE* file, char* buffer)
{
	int type = buffer ? _IOFBF : _IONBF;
	setvbuf(file, buffer, type, BUFSIZ);
}

int setvbuf(FILE* file, char* buffer, int type, size_t size)
{
	if (file->fd == -1)
	{
		errno = EBADF;
		return -1;
	}

	if (buffer == nullptr)
	{
		buffer = reinterpret_cast<char*>(file->inline_buffer_storage);
		size = BAN::Math::min<size_t>(size, BUFSIZ);
	}

	file->buffer_type = type;
	file->buffer_size = size;
	file->buffer = reinterpret_cast<unsigned char*>(buffer);

	return 0;
}

int snprintf(char* buffer, size_t max_size, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vsnprintf(buffer, max_size, format, arguments);
	va_end(arguments);
	return ret;
}

int sprintf(char* buffer, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vsprintf(buffer, format, arguments);
	va_end(arguments);
	return ret;
}

int sscanf(const char* s, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vsscanf(s, format, arguments);
	va_end(arguments);
	return ret;
}

// TODO
char* tempnam(const char*, const char*);

// TODO
FILE* tmpfile(void);

char* tmpnam(char* storage)
{
	static int s_counter = rand();
	static char s_storage[PATH_MAX];
	if (storage == nullptr)
		storage = s_storage;
	for (int i = 0; i < TMP_MAX; i++)
	{
		sprintf(storage, "/tmp/tmp_%04x", s_counter);
		s_counter = rand();

		struct stat st;
		if (stat(storage, &st) == -1 && errno == ENOENT)
			break;
	}
	return storage;
}

int ungetc(int c, FILE* stream)
{
	dwarnln("FIXME: ungetc({}, {})", c, stream);
	ASSERT_NOT_REACHED();
}

int vfprintf(FILE* file, const char* format, va_list arguments)
{
	ScopeLock _(file);
	return printf_impl(format, arguments, [](int c, void* file) { return putc_unlocked(c, static_cast<FILE*>(file)); }, file);
}

int vfscanf(FILE* file, const char* format, va_list arguments)
{
	ScopeLock _(file);
	return scanf_impl(format, arguments, [](void* file) { return getc_unlocked(static_cast<FILE*>(file)); }, file);
}

int vprintf(const char* format, va_list arguments)
{
	return vfprintf(stdout, format, arguments);
}

int vscanf(const char* format, va_list arguments)
{
	return vfscanf(stdin, format, arguments);
}

int vsnprintf(char* buffer, size_t max_size, const char* format, va_list arguments)
{
	if (buffer == nullptr)
		return printf_impl(format, arguments, [](int, void*) { return 0; }, nullptr);

	struct print_info
	{
		char* buffer;
		size_t remaining;
	};
	print_info info { buffer, max_size };

	int ret = printf_impl(format, arguments,
		[](int c, void* _info)
		{
			print_info* info = (print_info*)_info;
			if (info->remaining == 1)
			{
				*info->buffer = '\0';
				info->remaining = 0;
			}
			else if (info->remaining)
			{
				*info->buffer = c;
				info->buffer++;
				info->remaining--;
			}
			return 0;
		}, &info
	);

	*info.buffer = '\0';
	return ret;
}

int vsprintf(char* buffer, const char* format, va_list arguments)
{
	if (buffer == nullptr)
		return printf_impl(format, arguments, [](int, void*) { return 0; }, nullptr);

	int ret = printf_impl(format, arguments,
		[](int c, void* _buffer)
		{
			*(*(char**)_buffer)++ = c;
			return 0;
		}, &buffer
	);

	*buffer = '\0';
	return ret;
}

int vsscanf(const char* s, const char* format, va_list arguments)
{
	return scanf_impl(format, arguments,
		[](void* data) -> int
		{
			char ret = **static_cast<char**>(data);
			(*static_cast<char**>(data))++;
			if (ret == '\0')
				return -1;
			return ret;
		}, &s
	);
}
