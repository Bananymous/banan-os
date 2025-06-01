#include <BAN/UTF8.h>

#include <errno.h>
#include <wchar.h>

struct FILEScopeLock
{
	FILEScopeLock(FILE* stream)
		: m_stream(stream)
	{
		flockfile(m_stream);
	}
	~FILEScopeLock()
	{
		funlockfile(m_stream);
	}
	FILE* m_stream;
};

wint_t getwc(FILE* stream)
{
	return fgetwc(stream);
}

wint_t fgetwc(FILE* stream)
{
	FILEScopeLock _(stream);

	char buffer[4];

	buffer[0] = getc_unlocked(stream);
	if (buffer[0] == EOF)
		return WEOF;

	const auto length = BAN::UTF8::byte_length(buffer[0]);
	if (length == BAN::UTF8::invalid)
	{
		errno = EILSEQ;
		return WEOF;
	}

	for (uint32_t i = 1; i < length; i++)
		if ((buffer[i] = getc_unlocked(stream)) == EOF)
			return WEOF;

	const auto ret = BAN::UTF8::to_codepoint(buffer);
	if (ret == BAN::UTF8::invalid)
	{
		errno = EILSEQ;
		return WEOF;
	}

	return ret;
}

wint_t putwc(wchar_t wc, FILE* stream)
{
	return fputwc(wc, stream);
}

wint_t fputwc(wchar_t wc, FILE* stream)
{
	char buffer[4];
	if (!BAN::UTF8::from_codepoints(&wc, 1, buffer))
	{
		errno = EILSEQ;
		return WEOF;
	}

	FILEScopeLock _(stream);

	const auto bytes = BAN::UTF8::byte_length(buffer[0]);
	for (uint32_t i = 0; i < bytes; i++)
		if (putc_unlocked(buffer[i], stream) == EOF)
			return WEOF;

	return wc;
}

wint_t ungetwc(wint_t wc, FILE* stream)
{
	char buffer[4];
	if (!BAN::UTF8::from_codepoints(&wc, 1, buffer))
	{
		errno = EILSEQ;
		return WEOF;
	}

	FILEScopeLock _(stream);

	const auto bytes = BAN::UTF8::byte_length(buffer[0]);
	for (uint32_t i = 0; i < bytes; i++)
	{
		if (ungetc(buffer[i], stream) != EOF)
			continue;
		for (uint32_t j = 0; j < i; j++)
			fgetc(stream);
		return WEOF;
	}

	return wc;
}
