#include <bits/printf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct FILE
{
	int fd			{ -1 };
	bool eof		{ false };
	bool error		{ false };

	unsigned char buffer[BUFSIZ];
	uint32_t buffer_index { 0 };
};

static FILE s_files[FOPEN_MAX] {
	{ .fd = STDIN_FILENO  },
	{ .fd = STDOUT_FILENO },
	{ .fd = STDERR_FILENO },
};

FILE* stdin  = &s_files[0];
FILE* stdout = &s_files[1];
FILE* stderr = &s_files[2];

extern "C" void _init_stdio()
{
	char tty[L_ctermid];
	ctermid(tty);
	if (open(tty, O_RDONLY) != 0) _exit(1);
	if (open(tty, O_WRONLY) != 1) _exit(1);
	if (open(tty, O_WRONLY) != 2) _exit(1);
}

void clearerr(FILE* file)
{
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
	if (close(file->fd) == -1)
		return EOF;
	file->fd = -1;
	return 0;
}

// TODO
FILE* fdopen(int, const char*);

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

int fgetpos(FILE* file, fpos_t* pos)
{
	off_t offset = ftello(file);
	if (offset == -1)
		return -1;
	*pos = offset;
	return 0;
}

char* fgets(char* str, int size, FILE* file)
{
	if (size == 0)
		return str;
	int c = fgetc(file);
	if (c == EOF)
		return nullptr;
	str[0] = c;
	for (int i = 1; i < size - 1; i++)
	{
		str[i] = fgetc(file);
		if (str[i] == EOF)
		{
			str[i] = '\0';
			return nullptr;
		}
		if (str[i] == '\n')
		{
			str[i + 1] = '\0';
			return str;
		}
	}
	str[size - 1] = '\0';
	return str;
}

// TODO
int fileno(FILE*);

// TODO
void flockfile(FILE*);

FILE* fopen(const char* pathname, const char* mode)
{
	uint8_t flags = 0;
	if (mode[0] == 'r')
		flags |= O_RDONLY;
	else if (mode[0] == 'w')
		flags |= O_WRONLY | O_CREAT | O_TRUNC;
	else if (mode[0] == 'a')
		flags |= O_WRONLY | O_CREAT | O_APPEND;
	else
	{
		errno = EINVAL;
		return nullptr;
	}

	if (mode[1] && mode[2] && mode[1] == mode[2])
	{
		errno = EINVAL;
		return nullptr;
	}

	for (int i = 1; i <= 2; i++)
	{
		if (mode[i] == 0)
			break;
		else if (mode[i] == '+')
			flags |= O_RDWR;
		else if (mode[i] == 'b')
			continue;
		else
		{
			errno = EINVAL;
			return nullptr;
		}
	}

	int fd = open(pathname, flags);
	if (fd == -1)
		return nullptr;

	for (int i = 0; i < FOPEN_MAX; i++)
	{
		if (s_files[i].fd == -1)
		{
			s_files[i] = { .fd = fd };
			return &s_files[i];
		}
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
	file->buffer[file->buffer_index++] = c;
	if (c == '\n' || file->buffer_index == sizeof(file->buffer))
		if (fflush(file) == EOF)
			return EOF;
	return (unsigned char)c;
}

int fputs(const char* str, FILE* file)
{
	while (*str)
	{
		if (fputc(*str, file) == EOF)
			return EOF;
		str++;
	}
	return 0;
}

size_t fread(void* buffer, size_t size, size_t nitems, FILE* file)
{
	if (file->eof || nitems * size == 0)
		return 0;
	long ret = syscall(SYS_READ, file->fd, buffer, size * nitems);
	if (ret < 0)
	{
		file->error = true;
		return 0;
	}
	if (ret == 0)
		file->eof = true;
	return ret / size;
}

// TODO
FILE* freopen(const char*, const char*, FILE*);

// TODO
int fscanf(FILE*, const char*, ...);

int fseek(FILE* file, long offset, int whence)
{
	return fseeko(file, offset, whence);
}

int fseeko(FILE* file, off_t offset, int whence)
{
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
	long ret = syscall(SYS_TELL, file->fd);
	if (ret < 0)
		return -1;
	return ret;
}

// TODO
int ftrylockfile(FILE*);

// TODO
void funlockfile(FILE*);

size_t fwrite(const void* buffer, size_t size, size_t nitems, FILE* file)
{
	unsigned char* ubuffer = (unsigned char*)buffer;
	for (size_t byte = 0; byte < nitems * size; byte++)
		if (fputc(ubuffer[byte], file) == EOF)
			return byte / size;
	return nitems;
}

int getc(FILE* file)
{
	return fgetc(file);
}

int getchar(void)
{
	return getc(stdin);
}

// TODO
int getc_unlocked(FILE*);

// TODO
int getchar_unlocked(void);

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

// TODO
int pclose(FILE*);

void perror(const char* string)
{
	if (string && *string)
	{
		fputs(string, stderr);
		fputs(": ", stderr);
	}
	fputs(strerror(errno), stderr);
	fputc('\n', stderr);
	stderr->error = true;
}

// TODO
FILE* popen(const char*, const char*);

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

// TODO
int putc_unlocked(int, FILE*);

// TODO
int putchar_unlocked(int);

int puts(const char* string)
{
	if (fputs(string, stdout) == EOF)
		return EOF;
	if (fputc('\n', stdout) == EOF)
		return EOF;
	return 0;
}

// TODO
int remove(const char*);

// TODO
int rename(const char*, const char*);

// TODO
void rewind(FILE*);

// TODO
int scanf(const char*, ...);

// TODO
void setbuf(FILE*, char*);

// TODO
int setvbuf(FILE*, char*, int, size_t);

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

// TODO
int sscanf(const char*, const char*, ...);

// TODO
char* tempnam(const char*, const char*);

// TODO
FILE* tmpfile(void);

// TODO
char* tmpnam(char*);

// TODO
int ungetc(int, FILE*);

int vfprintf(FILE* file, const char* format, va_list arguments)
{
	return printf_impl(format, arguments, [](int c, void* file) { return fputc(c, (FILE*)file); }, file);
}

// TODO
int vfscanf(FILE*, const char*, va_list);

int vprintf(const char* format, va_list arguments)
{
	return vfprintf(stdout, format, arguments);
}

// TODO
int vscanf(const char*, va_list);

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

// TODO
int vsscanf(const char*, const char*, va_list);
