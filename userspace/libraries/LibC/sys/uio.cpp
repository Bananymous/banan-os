#include <pthread.h>
#include <sys/uio.h>
#include <unistd.h>

ssize_t readv(int fildes, const struct iovec* iov, int iovcnt)
{
	pthread_testcancel();

	size_t result = 0;
	for (int i = 0; i < iovcnt; i++)
	{
		uint8_t* base = static_cast<uint8_t*>(iov[i].iov_base);

		size_t nread = 0;
		while (nread < iov[i].iov_len)
		{
			const ssize_t ret = read(fildes, base + nread, iov[i].iov_len - nread);
			if (ret == -1 && result == 0)
				return -1;
			if (ret <= 0)
				return result;
			nread += ret;
		}
		result += nread;
	}
	return result;
}

ssize_t writev(int fildes, const struct iovec* iov, int iovcnt)
{
	pthread_testcancel();

	size_t result = 0;
	for (int i = 0; i < iovcnt; i++)
	{
		const uint8_t* base = static_cast<const uint8_t*>(iov[i].iov_base);

		size_t nwrite = 0;
		while (nwrite < iov[i].iov_len)
		{
			const ssize_t ret = write(fildes, base + nwrite, iov[i].iov_len - nwrite);
			if (ret == -1 && result == 0)
				return -1;
			if (ret <= 0)
				return result;
			nwrite += ret;
		}
		result += nwrite;
	}
	return result;
}
