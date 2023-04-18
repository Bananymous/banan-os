#pragma once

#include <BAN/Formatter.h>
#include <BAN/Variant.h>

#include <errno.h>
#include <string.h>

#if defined(__is_kernel)
	#include <kernel/Panic.h>
	#include <kernel/Errors.h>
	#define MUST(expr)	({ auto e = expr; if (e.is_error()) Kernel::panic("{}", e.error()); e.release_value(); })
#else
	#define MUST(expr)	({ auto e = expr; assert(!e.is_error()); e.release_value(); })
#endif

#define TRY(expr) ({ auto e = expr; if (e.is_error()) return e.release_error(); e.release_value(); })

namespace BAN
{

	class Error
	{
	private:
		static constexpr uint32_t kernel_error_mask = 0x80000000;

	public:
		static Error from_error_code(Kernel::ErrorCode error)
		{
			return Error((uint32_t)error | kernel_error_mask);
		}
		static Error from_errno(int error)
		{
			return Error(error);
		}

		Kernel::ErrorCode kernel_error() const
		{
			return (Kernel::ErrorCode)(m_error_code & ~kernel_error_mask);
		}

		uint32_t get_error_code() const { return m_error_code; }
		BAN::StringView get_message() const
		{
			if (m_error_code & kernel_error_mask)
				return Kernel::error_string(kernel_error());
			return strerror(m_error_code);
		}

	private:
		Error(uint32_t error)
			: m_error_code(error)
		{}

		uint32_t m_error_code;
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

		bool is_error() const			{ return m_data.template has<Error>(); }
		const Error& error() const		{ return m_data.template get<Error>(); }
		Error& error()					{ return m_data.template get<Error>(); }
		const T& value() const			{ return m_data.template get<T>(); }
		T& value()						{ return m_data.template get<T>(); }

		Error release_error()			{ return move(error()); m_data.clear(); }
		T release_value()				{ return move(value()); m_data.clear(); }

	private:
		Variant<Error, T> m_data;
	};

	template<lvalue_reference T>
	class [[nodiscard]] ErrorOr<T>
	{
	public:
		ErrorOr(T value)
		{
			m_data.template set<T>(value);
		}
		ErrorOr(Error&& error)
			: m_data(move(error))
		{ }
		ErrorOr(const Error& error)
			: m_data(error)
		{ }

		bool is_error() const			{ return m_data.template has<Error>(); }
		Error& error()					{ return m_data.template get<Error>(); }
		const Error& error() const		{ return m_data.template get<Error>(); }
		T value()						{ return m_data.template get<T>(); }

		Error release_error()			{ return move(error()); m_data.clear(); }
		T release_value()				{ return value(); m_data.clear(); }

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

		Error release_error()			{ return move(m_data); }
		void release_value()			{ }

	private:
		Error m_data { Error::from_error_code(Kernel::ErrorCode::None) };
		bool m_has_error { false };
	};

}

namespace BAN::Formatter
{
	template<typename F>
	void print_argument(F putc, const Error& error, const ValueFormat& format)
	{
		print_argument(putc, error.get_message(), format);
	}
}
