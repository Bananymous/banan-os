#pragma once

#include <BAN/Formatter.h>

#include <string.h>

#if defined(__is_kernel)
	#include <kernel/Panic.h>
	#define MUST(error)	({ auto e = error; if (e.is_error()) Kernel::panic("{}", e.get_error()); e.value(); })
	#define ASSERT(cond) do { if (!(cond)) Kernel::panic("ASSERT("#cond") failed"); } while(false)
#else
	#error "NOT IMPLEMENTED"
#endif

#define TRY(error) ({ auto e = error; if (e.is_error()) return e.get_error(); e.value(); })

namespace BAN
{

	class Error
	{
	public:
		static Error from_string(const char* message)
		{
			Error result;
			strncpy(result.m_message, message, sizeof(m_message));
			result.m_message[sizeof(result.m_message) - 1] = '\0';
			result.m_error_code = 0xFF;
			return result;
		}

		uint8_t get_error_code() const { return m_error_code; }
		const char* get_message() const { return m_message; }

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
		template<typename S> ErrorOr(const ErrorOr<S>& other) : ErrorOr(other.get_error()) {}
		~ErrorOr()											{ is_error() ? (delete reinterpret_cast<Error*>(m_data)) : (delete reinterpret_cast<T*>(m_data)); }

		bool is_error() const			{ return m_has_error; }
		const Error& get_error() const	{ return *reinterpret_cast<Error*>(m_data); }
		T& value()						{ return *reinterpret_cast<T*>(m_data); }

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

		bool is_error() const			{ return m_has_error; }
		const Error& get_error() const	{ return m_error; }
		void value()					{ }

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
		if (error.get_error_code() == 0xFF)
			print(putc, error.get_message());
		else
			print(putc, "{} ({})", error.get_message(), error.get_error_code());
	}
}
