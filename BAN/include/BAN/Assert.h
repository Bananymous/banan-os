#pragma once

#include <BAN/Traits.h>

#if defined(__is_kernel)
	#include <kernel/Panic.h>

	#define ASSERT(cond)									\
		do {												\
			if (!(cond))									\
				Kernel::panic("ASSERT(" #cond ") failed");	\
		} while (false)

	#define __ASSERT_BIN_OP(lhs, rhs, name, op)														\
		do {																						\
			auto&& _lhs = lhs;																		\
			auto&& _rhs = rhs;																		\
			if (!(_lhs op _rhs))																	\
					Kernel::panic(name "(" #lhs ", " #rhs ") ({} " #op " {}) failed", _lhs, _rhs);	\
		} while (false)

	#define ASSERT_LT(lhs, rhs)		__ASSERT_BIN_OP(lhs, rhs, "ASSERT_LT",	<)
	#define ASSERT_LTE(lhs, rhs)	__ASSERT_BIN_OP(lhs, rhs, "ASSERT_LTE",	<=)
	#define ASSERT_GT(lhs, rhs)		__ASSERT_BIN_OP(lhs, rhs, "ASSERT_GT",	>)
	#define ASSERT_GTE(lhs, rhs)	__ASSERT_BIN_OP(lhs, rhs, "ASSERT_GTE",	>=)
	#define ASSERT_EQ(lhs, rhs)		__ASSERT_BIN_OP(lhs, rhs, "ASSERT_EQ",	==)
	#define ASSERT_NEQ(lhs, rhs)	__ASSERT_BIN_OP(lhs, rhs, "ASSERT_NEQ",	!=)
	#define ASSERT_NOT_REACHED() Kernel::panic("ASSERT_NOT_REACHED() failed")
#else
	#include <assert.h>
	#define ASSERT(cond) assert((cond) && "ASSERT("#cond") failed")
	#define ASSERT_NOT_REACHED() do { assert(false && "ASSERT_NOT_REACHED() failed"); __builtin_unreachable(); } while (false)
#endif
