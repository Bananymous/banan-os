#pragma once

#include <BAN/Formatter.h>
#include <BAN/Variant.h>

#include <string.h>

#if defined(__is_kernel)
	#include <kernel/Panic.h>
	#define MUST(expr)	({ auto e = expr; if (e.is_error()) Kernel::panic("{}", e.error()); e.release_value(); })
#else
	#error "NOT IMPLEMENTED"
#endif

#define TRY(expr) ({ auto e = expr; if (e.is_error()) return e.error(); e.release_value(); })

namespace BAN
{

	class Error
	{
	public:
		static Error from_string(const char* message)
		{
			static_assert(sizeof(message) < 128);
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
	class [[nodiscard]] ErrorOr
	{
	public:
		ErrorOr(const T& value)
			: m_data(value)
		{}
		ErrorOr(T&& value)
			: m_data(move(value))
		{}
		ErrorOr(const Error& error)
			: m_data(error)
		{}
		ErrorOr(Error&& error)
			: m_data(move(error))
		{}
		template<typename U>
		ErrorOr(const ErrorOr<U>& other)
			: m_data(other.m_data)
		{}
		template<typename U>
		ErrorOr(ErrorOr<U>&& other)
			: m_data(move(other.m_data))
		{}
		template<typename U>
		ErrorOr<T>& operator=(const ErrorOr<U>& other)
		{
			m_data = other.m_data;
			return *this;
		}
		template<typename U>
		ErrorOr<T>& operator=(ErrorOr<U>&& other)
		{
			m_data = move(other.m_data);
			return *this;
		}

		bool is_error() const			{ return m_data.template is<Error>(); }
		const Error& error() const		{ return m_data.template get<Error>(); }
		Error& error()					{ return m_data.template get<Error>(); }
		const T& value() const			{ return m_data.template get<T>(); }
		T& value()						{ return m_data.template get<T>(); }
		T release_value()				{ return move(value()); m_data.clear(); }

	private:
		Variant<Error, T> m_data;
	};

	template<>
	class [[nodiscard]] ErrorOr<void>
	{
	public:
		ErrorOr()														{}
		ErrorOr(const Error& error) : m_data(error), m_has_error(true)	{}
		ErrorOr(Error&& error) : m_data(move(error)), m_has_error(true)	{}

		bool is_error() const			{ return m_has_error; }
		Error& error()					{ return m_data; }
		const Error& error() const		{ return m_data; }
		void value()					{ }
		void release_value()			{ m_data = Error(); }

	private:
		Error m_data;
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
