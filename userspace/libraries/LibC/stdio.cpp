#include <BAN/Assert.h>
#include <BAN/Atomic.h>
#include <BAN/Math.h>
#include <BAN/PlacementNew.h>

#include <bits/printf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <scanf_impl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct FILE
{
	int fd;
	mode_t mode;
	bool eof;
	bool error;

	int pid;

	unsigned char inline_buffer[BUFSIZ];
	unsigned char* buffer;
	uint32_t buffer_rd_size; // 0 write buffer
	uint32_t buffer_size;
	uint32_t buffer_idx;
	int buffer_type;

	unsigned char unget_buffer[12];
	uint32_t unget_buf_idx;

	// TODO: use recursive pthread_mutex when implemented?
	//       this storage hack is to keep FILE pod (init order)
	BAN::Atomic<pthread_t>& locker() { return *reinterpret_cast<BAN::Atomic<pthread_t>*>(locker_storage); }
	unsigned char locker_storage[sizeof(pthread_t)];
	uint32_t lock_depth;
};
static_assert(BAN::is_pod_v<FILE>);

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

	FILE* const m_file;
};

static FILE s_files[FOPEN_MAX];

FILE* stdin  = &s_files[0];
FILE* stdout = &s_files[1];
FILE* stderr = &s_files[2];
FILE* stddbg = &s_files[3];

static void init_closed_file(FILE* file)
{
	file->fd             = -1;
	file->mode           = 0;
	file->eof            = false;
	file->error          = false;
	file->pid            = -1;
	file->buffer         = file->inline_buffer;
	file->buffer_size    = sizeof(file->inline_buffer);
	file->buffer_idx     = 0;
	file->buffer_type    = _IOFBF;
	file->buffer_rd_size = 0;
	file->unget_buf_idx  = 0;
}

static int drop_read_buffer(FILE* file)
{
	const off_t bytes_remaining = file->buffer_rd_size - file->buffer_idx;
	if (bytes_remaining > 0)
		if (syscall(SYS_SEEK, file->fd, -bytes_remaining, SEEK_CUR) == -1)
			return EOF;
	file->buffer_rd_size = 0;
	file->buffer_idx = 0;
	return 0;
}

__attribute__((constructor))
static void _init_stdio()
{
	for (size_t i = 0; i < FOPEN_MAX; i++)
	{
		init_closed_file(&s_files[i]);

		new (&s_files[i].locker()) BAN::Atomic<pthread_t>();
		s_files[i].locker() = -1;
		s_files[i].lock_depth = 0;
	}

	s_files[STDIN_FILENO].fd          = STDIN_FILENO;
	s_files[STDIN_FILENO].mode        = O_RDONLY;
	s_files[STDIN_FILENO].buffer_type = _IOLBF;

	s_files[STDOUT_FILENO].fd          = STDOUT_FILENO;
	s_files[STDOUT_FILENO].mode        = O_WRONLY;
	s_files[STDOUT_FILENO].buffer_type = _IOLBF;

	s_files[STDERR_FILENO].fd          = STDERR_FILENO;
	s_files[STDERR_FILENO].mode        = O_WRONLY;
	s_files[STDERR_FILENO].buffer_type = _IONBF;

	s_files[STDDBG_FILENO].fd          = STDDBG_FILENO;
	s_files[STDDBG_FILENO].mode        = O_WRONLY;
	s_files[STDDBG_FILENO].buffer_type = _IOLBF;
}

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

int dprintf(int fildes, const char* __restrict format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int ret = vdprintf(fildes, format, arguments);
	va_end(arguments);
	return ret;
}

int fclose(FILE* file)
{
	ScopeLock _(file);
	if (fflush(file) == EOF)
		return EOF;
	if (close(file->fd) == -1)
		return EOF;
	init_closed_file(file);
	return 0;
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

	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = fd;
		s_files[i].mode = mode & O_ACCMODE;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer);
		ASSERT(s_files[i].buffer_size == BUFSIZ);
		ASSERT(s_files[i].buffer_idx == 0);
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
		int ret = 0;
		for (int i = 0; i < FOPEN_MAX; i++)
			if (s_files[i].fd != -1)
				if (int err = fflush(&s_files[i]); err != 0)
					ret = err;
		return ret;
	}

	ScopeLock _(file);

	if (file->fd == -1)
	{
		errno = EBADF;
		return EOF;
	}

	file->unget_buf_idx = 0;

	if (file->buffer_rd_size)
		return drop_read_buffer(file);

	size_t written = 0;
	while (written < file->buffer_idx)
	{
		ssize_t nwrite = write(file->fd, file->buffer + written, file->buffer_idx - written);
		if (nwrite < 0)
		{
			file->error = true;
			return EOF;
		}
		written += nwrite;
	}

	file->buffer_idx = 0;

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
	if (fp->fd == -1)
	{
		errno = EBADF;
		return -1;
	}
	return fp->fd;
}

void flockfile(FILE* fp)
{
	const pthread_t tid = pthread_self();

	pthread_t expected = -1;
	while (!fp->locker().compare_exchange(expected, tid, BAN::MemoryOrder::memory_order_acq_rel))
	{
		if (expected == tid)
			break;
		sched_yield();
		expected = -1;
	}

	fp->lock_depth++;
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

	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = fd;
		s_files[i].mode = mode & O_ACCMODE;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer);
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
	if (file->eof || size == 0 || nitems == 0)
		return 0;

	auto* ubuffer = static_cast<unsigned char*>(buffer);
	for (size_t item = 0; item < nitems; item++)
	{
		for (size_t byte = 0; byte < size; byte++)
		{
			int ch = getc_unlocked(file);
			if (ch == EOF)
				return item;
			*ubuffer++ = ch;
		}
	}

	return nitems;
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
	if (fflush(file) == EOF)
		return -1;
	if (syscall(SYS_SEEK, file->fd, offset, whence) == -1)
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
	auto offset = syscall(SYS_TELL, file->fd);
	if (offset == -1)
		return -1;
	if (file->buffer_rd_size)
		offset -= file->buffer_rd_size - file->buffer_idx;
	return offset - file->unget_buf_idx;
}

int ftrylockfile(FILE* fp)
{
	const pthread_t tid = pthread_self();

	pthread_t expected = -1;
	if (!fp->locker().compare_exchange(expected, tid, BAN::MemoryOrder::memory_order_acq_rel))
		if (expected != tid)
			return 1;

	fp->lock_depth++;
	return 0;
}

void funlockfile(FILE* fp)
{
	ASSERT(fp->locker() == pthread_self());
	ASSERT(fp->lock_depth > 0);
	if (--fp->lock_depth == 0)
		fp->locker().store(-1, BAN::MemoryOrder::memory_order_release);
}

size_t fwrite(const void* buffer, size_t size, size_t nitems, FILE* file)
{
	ScopeLock _(file);
	if (size == 0 || nitems == 0)
		return 0;

	const auto* ubuffer = static_cast<const unsigned char*>(buffer);
	for (size_t item = 0; item < nitems; item++)
		for (size_t byte = 0; byte < size; byte++)
			if (putc_unlocked(*ubuffer++, file) == EOF)
				return item;
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
	if (file->fd == -1 || !(file->mode & O_RDONLY))
	{
		errno = EBADF;
		return EOF;
	}

	if (file->eof)
		return EOF;

	// read characters from ungetc
	if (file->unget_buf_idx)
	{
		file->unget_buf_idx--;
		return file->unget_buffer[file->unget_buf_idx];
	}

	// read from unbuffered file
	if (file->buffer_type == _IONBF)
	{
		unsigned char ch;
		if (ssize_t nread = read(file->fd, &ch, 1); nread <= 0)
		{
			((nread == 0) ? file->eof : file->error) = true;
			return EOF;
		}
		return ch;
	}

	// flush writable data
	if (file->buffer_rd_size == 0 && file->buffer_idx)
		if (fflush(file) == EOF)
			return EOF;

	// buffered read
	if (file->buffer_idx < file->buffer_rd_size)
	{
		unsigned char ch = file->buffer[file->buffer_idx];
		file->buffer_idx++;
		return ch;
	}

	if (drop_read_buffer(file) == EOF)
		return EOF;

	// read into buffer
	ssize_t nread = read(file->fd, file->buffer, file->buffer_size);
	if (nread <= 0)
	{
		((nread == 0) ? file->eof : file->error) = true;
		return EOF;
	}
	file->buffer_rd_size = nread;
	file->buffer_idx = 1;
	return file->buffer[0];
}

int getchar_unlocked(void)
{
	return getc_unlocked(stdin);
}

ssize_t getdelim(char** __restrict lineptr, size_t* __restrict n, int delimeter, FILE* __restrict stream)
{
	if (n == nullptr || lineptr == nullptr)
	{
		errno = EINVAL;
		return -1;
	}

	ScopeLock _(stream);

	size_t capacity = *lineptr ? *n : 0;
	size_t length = 0;

	for (;;)
	{
		if (length + 2 > capacity)
		{
			const size_t new_capacity = BAN::Math::max(capacity * 2, length + 2);
			void* temp = realloc(*lineptr, new_capacity);
			if (temp == nullptr)
				return -1;
			*lineptr = static_cast<char*>(temp);
			capacity = new_capacity;
		}

		int ch = getc_unlocked(stream);
		if (ch == EOF)
		{
			(*lineptr)[length] = '\0';
			*n = length;
			if (ferror(stream) || length == 0)
				return -1;
			return length;
		}

		(*lineptr)[length++] = ch;

		if (ch == delimeter)
		{
			(*lineptr)[length] = '\0';
			*n = length;
			return length;
		}
	}
}

ssize_t getline(char** __restrict lineptr, size_t* __restrict n, FILE* __restrict stream)
{
	return getdelim(lineptr, n, '\n', stream);
}

char* gets(char* buffer)
{
	ScopeLock _(stdin);
	if (stdin->eof)
		return nullptr;

	int first = fgetc(stdin);
	if (first == EOF)
		return nullptr;

	unsigned char* ubuffer = reinterpret_cast<unsigned char*>(buffer);
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

	int stat;
	while (waitpid(file->pid, &stat, 0) == -1)
	{
		if (errno != EINTR)
		{
			stat = -1;
			break;
		}
	}

	(void)fclose(file);
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

	for (int i = 0; i < FOPEN_MAX; i++)
	{
		ScopeLock _(&s_files[i]);
		if (s_files[i].fd != -1)
			continue;
		s_files[i].fd = read ? fds[0] : fds[1];
		s_files[i].mode = (unsigned)(read ? O_RDONLY : O_WRONLY);
		s_files[i].pid = pid;
		ASSERT(s_files[i].buffer == s_files[i].inline_buffer);
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
	if (file->fd == -1 || !(file->mode & O_WRONLY))
	{
		errno = EBADF;
		return EOF;
	}

	file->unget_buf_idx = 0;
	if (file->buffer_rd_size && drop_read_buffer(file) == EOF)
		return EOF;

	if (file->buffer_type == _IONBF)
	{
		ssize_t nwrite = write(file->fd, &c, 1);
		if (nwrite == -1)
			file->error = true;
		if (nwrite <= 0)
			return EOF;
		return (unsigned char)c;
	}

	file->buffer[file->buffer_idx] = c;
	file->buffer_idx++;
	if ((file->buffer_type == _IOLBF && c == '\n') || file->buffer_idx >= file->buffer_size)
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

	if (size == 0)
		type = _IONBF;

	unsigned char* ubuffer = reinterpret_cast<unsigned char*>(buffer);
	if (ubuffer == nullptr)
	{
		ubuffer = file->inline_buffer;
		size = BAN::Math::min<size_t>(size, sizeof(file->inline_buffer));
	}

	ASSERT(file->buffer_rd_size == 0);
	ASSERT(file->buffer_idx == 0);
	file->buffer_type = type;
	file->buffer_size = size;
	file->buffer      = ubuffer;

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

FILE* tmpfile(void)
{
	for (;;)
	{
		char path[PATH_MAX];
		if (tmpnam(path) == nullptr)
			return nullptr;

		int fd = open(path, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd == -1)
		{
			if (errno == EEXIST)
				continue;
			return nullptr;
		}

		(void)unlink(path);

		FILE* fp = fdopen(fd, "wb+");
		if (fp != nullptr)
			return fp;

		close(fd);
		return nullptr;
	}
}

char* tmpnam(char* storage)
{
	static char s_storage[PATH_MAX];
	if (storage == nullptr)
		storage = s_storage;
	for (int i = 0; i < TMP_MAX; i++)
	{
		sprintf(storage, "/tmp/tmp_%04x", rand());

		struct stat st;
		if (stat(storage, &st) == -1 && errno == ENOENT)
			return storage;
	}
	return nullptr;
}

int ungetc_unlocked(int c, FILE* stream)
{
	if (stream->fd == -1)
	{
		errno = EBADF;
		return EOF;
	}

	if (c == EOF || stream->unget_buf_idx >= sizeof(stream->unget_buffer))
	{
		errno = EINVAL;
		return EOF;
	}

	stream->unget_buffer[stream->unget_buf_idx] = c;
	stream->unget_buf_idx++;
	stream->eof = false;
	return (unsigned char)c;
}

int ungetc(int c, FILE* stream)
{
	ScopeLock _(stream);
	return ungetc_unlocked(c, stream);
}

int vdprintf(int fildes, const char* __restrict format, va_list arguments)
{
	struct print_info
	{
		int fildes;
		size_t offset;
		char buffer[512];
	};

	print_info info {
		.fildes = fildes,
		.offset = 0,
		.buffer = {},
	};

	const int ret = printf_impl(format, arguments,
		[](int c, void* _info) -> int
		{
			auto* info = static_cast<print_info*>(_info);
			info->buffer[info->offset++] = c;
			if (info->offset >= sizeof(info->buffer))
			{
				write(info->fildes, info->buffer, info->offset);
				info->offset = 0;
			}
			return 0;
		}, static_cast<void*>(&info)
	);

	if (info.offset)
		write(info.fildes, info.buffer, info.offset);

	return ret;
}

int vfprintf(FILE* file, const char* format, va_list arguments)
{
	ScopeLock _(file);
	return printf_impl(format, arguments, [](int c, void* file) { return putc_unlocked(c, static_cast<FILE*>(file)); }, file);
}

int vfscanf(FILE* file, const char* format, va_list arguments)
{
	ScopeLock _(file);
	return scanf_impl(format, arguments,
		[](bool advance, void* data)
		{
			FILE* fp = static_cast<FILE*>(data);

			if (advance)
				getc_unlocked(fp);
			const int ret = getc_unlocked(fp);
			ungetc_unlocked(ret, fp);
			return ret;
		}, file
	);
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
		[](bool advance, void* data) -> int
		{
			const char** ptr = static_cast<const char**>(data);

			if (advance)
				(*ptr)++;
			const char ret = **ptr;
			if (ret == '\0')
				return EOF;
			return ret;
		}, &s
	);
}
