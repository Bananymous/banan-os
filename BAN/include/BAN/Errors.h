#pragma once

#include <BAN/Formatter.h>

#include <string.h>

#if defined(__is_kernel)
	#include <kernel/Panic.h>
	#define MUST(error)	({ auto e = error; if (e.IsError()) Kernel::Panic("{}", e.GetError()); e.Value(); })
	#define ASSERT(cond) do { if (!(cond)) Kernel::Panic("ASSERT("#cond") failed"); } while(false)
#else
	#error "NOT IMPLEMENTED"
#endif

#define TRY(error) ({ auto e = error; if (e.IsError()) return e.GetError(); e.Value(); })

namespace BAN
{

	class Error
	{
	public:
		static Error FromString(const char* message)
		{
			Error result;
			strncpy(result.m_message, message, sizeof(m_message));
			result.m_message[sizeof(result.m_message) - 1] = '\0';
			result.m_error_code = 0xFF;
			return result;
		}

		uint8_t GetErrorCode() const { return m_error_code; }
		const char* GetMessage() const { return m_message; }

	private:
		char m_message[128];
		uint8_t m_error_code;
	};

	template<typename T>
	class ErrorOr
	{
	public:
		ErrorOr(const T& value)		: m_has_error(false)	{ m_data = (void*)new T(value); }
		ErrorOr(const Error& error) : m_has_error(true)		{ m_data = (void*)new Error(error); }
		template<typename S> ErrorOr(const ErrorOr<S>& other) : ErrorOr(other.GetError()) {}
		~ErrorOr()											{ IsError() ? (delete reinterpret_cast<Error*>(m_data)) : (delete reinterpret_cast<T*>(m_data)); }

		bool IsError() const			{ return m_has_error; }
		const Error& GetError() const	{ return *reinterpret_cast<Error*>(m_data); }
		T& Value()						{ return *reinterpret_cast<T*>(m_data); }

	private:
		bool	m_has_error	= false;
		void*	m_data		= nullptr;
	};

	template<>
	class ErrorOr<void>
	{
	public:
		ErrorOr()										{ }
		ErrorOr(const Error& error) : m_error(error)	{ }
		~ErrorOr()										{ }

		bool IsError() const			{ return m_has_error; }
		const Error& GetError() const	{ return m_error; }
		void Value()					{ }

	private:
		Error m_error;
		bool m_has_error = false;
	};

}

namespace BAN::Formatter
{
	template<typename F>
	void print_argument_impl(F putc, const Error& error, const ValueFormat&)
	{
		if (error.GetErrorCode() == 0xFF)
			print(putc, error.GetMessage());
		else
			print(putc, "{} ({})", error.GetMessage(), error.GetErrorCode());
	}
}
