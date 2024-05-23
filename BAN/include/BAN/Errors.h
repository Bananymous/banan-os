#pragma once

#include <BAN/Formatter.h>
#include <BAN/Variant.h>

#include <errno.h>
#include <string.h>

#ifdef __is_kernel
	#include <kernel/Panic.h>
	#include <kernel/Errors.h>
	#define MUST(expr)		 ({ auto&& e = expr; if (e.is_error()) Kernel::panic("{}", e.error()); e.release_value(); })
	#define MUST_REF(expr)	*({ auto&& e = expr; if (e.is_error()) Kernel::panic("{}", e.error()); &e.release_value(); })
#else
	#include <assert.h>
	#define MUST(expr)		 ({ auto&& e = expr; assert(!e.is_error()); e.release_value(); })
	#define MUST_REF(expr)	*({ auto&& e = expr; assert(!e.is_error()); &e.release_value(); })
#endif

#define TRY(expr)		 ({ auto&& e = expr; if (e.is_error()) return e.release_error(); e.release_value(); })
#define TRY_REF(expr)	*({ auto&& e = expr; if (e.is_error()) return e.release_error(); &e.release_value(); })

namespace BAN
{

	class Error
	{
#ifdef __is_kernel
	private:
		static constexpr uint64_t kernel_error_mask = uint64_t(1) << 63;
#endif

	public:
#ifdef __is_kernel
		static Error from_error_code(Kernel::ErrorCode error)
		{
			return Error((uint64_t)error | kernel_error_mask);
		}
#endif
		static Error from_errno(int error)
		{
			return Error(error);
		}

#ifdef __is_kernel
		Kernel::ErrorCode kernel_error() const
		{
			return (Kernel::ErrorCode)(m_error_code & ~kernel_error_mask);
		}

		bool is_kernel_error() const
		{
			return m_error_code & kernel_error_mask;
		}
#endif

		uint64_t get_error_code() const { return m_error_code; }
		const char* get_message() const
		{
#ifdef __is_kernel
			if (m_error_code & kernel_error_mask)
				return Kernel::error_string(kernel_error());
#endif
			if (auto* desc = strerrordesc_np(m_error_code))
				return desc;
			return "Unknown error";
		}

	private:
		Error(uint64_t error)
			: m_error_code(error)
		{}

		uint64_t m_error_code;
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
		Error m_data { Error::from_errno(0) };
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
