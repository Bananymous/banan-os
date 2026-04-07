#pragma once

#define _ban_count_args_impl(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) _9
#define _ban_count_args(...) _ban_count_args_impl(__VA_ARGS__ __VA_OPT__(,) 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _ban_concat_impl(a, b) a##b
#define _ban_concat(a, b) _ban_concat_impl(a, b)

#define _ban_stringify_impl(x) #x
#define _ban_stringify(x) _ban_stringify_impl(x)

#define _ban_fe_0(f)
#define _ban_fe_1(f, _0)                                 f(0, _0)
#define _ban_fe_2(f, _0, _1)                             f(0, _0) f(1, _1)
#define _ban_fe_3(f, _0, _1, _2)                         f(0, _0) f(1, _1) f(2, _2)
#define _ban_fe_4(f, _0, _1, _2, _3)                     f(0, _0) f(1, _1) f(2, _2) f(3, _3)
#define _ban_fe_5(f, _0, _1, _2, _3, _4)                 f(0, _0) f(1, _1) f(2, _2) f(3, _3) f(4, _4)
#define _ban_fe_6(f, _0, _1, _2, _3, _4, _5)             f(0, _0) f(1, _1) f(2, _2) f(3, _3) f(4, _4) f(5, _5)
#define _ban_fe_7(f, _0, _1, _2, _3, _4, _5, _6)         f(0, _0) f(1, _1) f(2, _2) f(3, _3) f(4, _4) f(5, _5) f(6, _6)
#define _ban_fe_8(f, _0, _1, _2, _3, _4, _5, _6, _7)     f(0, _0) f(1, _1) f(2, _2) f(3, _3) f(4, _4) f(5, _5) f(6, _6) f(7, _7)
#define _ban_fe_9(f, _0, _1, _2, _3, _4, _5, _6, _7, _8) f(0, _0) f(1, _1) f(2, _2) f(3, _3) f(4, _4) f(5, _5) f(6, _6) f(7, _7) f(8, _8)
#define _ban_for_each(f, ...) _ban_concat(_ban_fe_, _ban_count_args(__VA_ARGS__))(f __VA_OPT__(,) __VA_ARGS__)

#define _ban_fe_comma_0(f)
#define _ban_fe_comma_1(f, _0)                                 f(0, _0)
#define _ban_fe_comma_2(f, _0, _1)                             f(0, _0), f(1, _1)
#define _ban_fe_comma_3(f, _0, _1, _2)                         f(0, _0), f(1, _1), f(2, _2)
#define _ban_fe_comma_4(f, _0, _1, _2, _3)                     f(0, _0), f(1, _1), f(2, _2), f(3, _3)
#define _ban_fe_comma_5(f, _0, _1, _2, _3, _4)                 f(0, _0), f(1, _1), f(2, _2), f(3, _3), f(4, _4)
#define _ban_fe_comma_6(f, _0, _1, _2, _3, _4, _5)             f(0, _0), f(1, _1), f(2, _2), f(3, _3), f(4, _4), f(5, _5)
#define _ban_fe_comma_7(f, _0, _1, _2, _3, _4, _5, _6)         f(0, _0), f(1, _1), f(2, _2), f(3, _3), f(4, _4), f(5, _5), f(6, _6)
#define _ban_fe_comma_8(f, _0, _1, _2, _3, _4, _5, _6, _7)     f(0, _0), f(1, _1), f(2, _2), f(3, _3), f(4, _4), f(5, _5), f(6, _6), f(7, _7)
#define _ban_fe_comma_9(f, _0, _1, _2, _3, _4, _5, _6, _7, _8) f(0, _0), f(1, _1), f(2, _2), f(3, _3), f(4, _4), f(5, _5), f(6, _6), f(7, _7), f(8, _8)
#define _ban_for_each_comma(f, ...) _ban_concat(_ban_fe_comma_, _ban_count_args(__VA_ARGS__))(f __VA_OPT__(,) __VA_ARGS__)

#define _ban_get_0(a0, ...)                                     a0
#define _ban_get_1(a0, a1, ...)                                 a1
#define _ban_get_2(a0, a1, a2, ...)                             a2
#define _ban_get_3(a0, a1, a2, a3, ...)                         a3
#define _ban_get_4(a0, a1, a2, a3, a4, ...)                     a4
#define _ban_get_5(a0, a1, a2, a3, a4, a5, ...)                 a5
#define _ban_get_6(a0, a1, a2, a3, a4, a5, a6, ...)             a6
#define _ban_get_7(a0, a1, a2, a3, a4, a5, a6, a7, ...)         a7
#define _ban_get_8(a0, a1, a2, a3, a4, a5, a6, a7, a8, ...)     a8
#define _ban_get_9(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, ...) a9
#define _ban_get(n, ...) _ban_concat(_ban_get_, n)(__VA_ARGS__)
