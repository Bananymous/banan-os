#pragma once

#define __ban_assert_stringify_helper(s) #s
#define __ban_assert_stringify(s) __ban_assert_stringify_helper(s)

#define ASSERT(cond)																						\
	(__builtin_expect(!(cond), 0)																			\
		? __ban_assertion_failed(__FILE__ ":" __ban_assert_stringify(__LINE__), "ASSERT(" #cond ") failed")	\
		: (void)0)

#define ASSERT_NOT_REACHED() ASSERT(false)

[[noreturn]] void __ban_assertion_failed(const char* location, const char* msg);
