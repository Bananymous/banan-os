#pragma once

namespace BAN
{

	template<typename T> struct remove_refenrece { using type = T; };
	template<typename T> struct remove_refenrece<T&> { using type =  T; };
	template<typename T> struct remove_refenrece<T&&> { using type = T; };
	template<typename T> using remove_reference_t = typename remove_refenrece<T>::type;

	template<typename T> struct remove_const { using type = T; };
	template<typename T> struct remove_const<const T> { using type = T; };
	template<typename T> using remove_const_t = typename remove_const<T>::type;

	template<typename T> struct remove_volatile { using type = T; };
	template<typename T> struct remove_volatile<volatile T> { using type = T; };
	template<typename T> using remove_volatile_t = typename remove_volatile<T>::type;

	template<typename T> struct remove_cv { using type = remove_volatile_t<remove_const_t<T>>; };
	template<typename T> using remove_cv_t = typename remove_cv<T>::type;

	template<typename T> struct remove_const_and_reference { using type = remove_const_t<remove_reference_t<T>>; };
	template<typename T> using remove_const_and_reference_t = typename remove_const_and_reference<T>::type;

	template<bool B, typename T = void> struct enable_if {};
	template<typename T> struct enable_if<true, T> { using type = T; };
	template<bool B, typename T = void> using enable_if_t = typename enable_if<B, T>::type;

	template<bool B, typename T> struct maybe_const { using type = T; };
	template<typename T> struct maybe_const<true, T> { using type = const T; };
	template<bool B, typename T> using maybe_const_t = typename maybe_const<B, T>::type;

	template<bool B, typename T1, typename T2> struct either_or { using type = T2; };
	template<typename T1, typename T2> struct either_or<true, T1, T2> { using type = T1; };
	template<bool B, typename T1, typename T2> using either_or_t = typename either_or<B, T1, T2>::type;

	struct true_type { static constexpr bool value = true; };
	struct false_type { static constexpr bool value = false; };

	template<typename T, typename S>	struct is_same			: false_type {};
	template<typename T>				struct is_same<T, T>	: true_type {};
	template<typename T, typename S> inline constexpr bool is_same_v = is_same<T, S>::value;

	template<typename T> struct is_lvalue_reference		: false_type {};
	template<typename T> struct is_lvalue_reference<T&>	: true_type {};
	template<typename T> inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;
	template<typename T> concept lvalue_reference = is_lvalue_reference_v<T>;

	template<typename T, typename... Args> struct is_constructible { static constexpr bool value = __is_constructible(T, Args...); };
	template<typename T, typename... Args> inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

	template<typename T> struct is_default_constructible { static constexpr bool value = is_constructible_v<T>; };
	template<typename T> inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

	template<typename T> struct is_copy_constructible { static constexpr bool value = is_constructible_v<T, const T&>; };
	template<typename T> inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

	template<typename T> struct is_move_constructible { static constexpr bool value = is_constructible_v<T, T&&>; };
	template<typename T> inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

	template<typename T> struct is_integral { static constexpr bool value = requires (T t, T* p, void (*f)(T)) { reinterpret_cast<T>(t); f(0); p + t; }; };
	template<typename T> inline constexpr bool is_integral_v = is_integral<T>::value;
	template<typename T> concept integral = is_integral_v<T>;

	template<typename T>	struct is_floating_point				: false_type {};
	template<>				struct is_floating_point<float>			: true_type {};
	template<>				struct is_floating_point<double>		: true_type {};
	template<>				struct is_floating_point<long double>	: true_type {};
	template<typename T> inline constexpr bool is_floating_point_v = is_floating_point<T>::value;
	template<typename T> concept floating_point = is_floating_point_v<T>;

	template<typename T> struct is_pointer						: false_type {};
	template<typename T> struct is_pointer<T*>					: true_type {};
	template<typename T> struct is_pointer<T* const>			: true_type {};
	template<typename T> struct is_pointer<T* volatile>			: true_type {};
	template<typename T> struct is_pointer<T* const volatile>	: true_type {};
	template<typename T> inline constexpr bool is_pointer_v = is_pointer<T>::value;
	template<typename T> concept pointer = is_pointer_v<T>;

	template<typename T> struct is_const			: false_type {};
	template<typename T> struct is_const<const T>	: true_type {};
	template<typename T> inline constexpr bool is_const_v = is_const<T>::value;

	template<typename T> struct is_arithmetic { static constexpr bool value = is_integral_v<T> || is_floating_point_v<T>; };
	template<typename T> inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

	template<typename Base, typename Derived> struct is_base_of { static constexpr bool value = __is_base_of(Base, Derived); };
	template<typename Base, typename Derived> inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

	template<typename T, T V> struct integral_constant { static constexpr T value = V; };
	template<typename T, T V > inline constexpr T integral_constant_v = integral_constant<T, V>::value;

	namespace detail
	{
		template<typename T, bool = is_arithmetic_v<T>> struct is_signed { static constexpr bool value = T(-1) < T(0); };
		template<typename T> struct is_signed<T, false> : false_type {};

		template<typename T, bool = is_arithmetic_v<T>> struct is_unsigned { static constexpr bool value = T(0) < T(-1); };
		template<typename T> struct is_unsigned<T, false> : false_type {};
	}
	template<typename T> struct is_signed : detail::is_signed<T> {};
	template<typename T> inline constexpr bool is_signed_v = is_signed<T>::value;
	template<typename T> concept signed_integral = is_signed_v<T> && is_integral_v<T>;

	template<typename T> struct is_unsigned : detail::is_unsigned<T> {};
	template<typename T> inline constexpr bool is_unsigned_v = is_unsigned<T>::value;
	template<typename T> concept unsigned_integral = is_unsigned_v<T> && is_integral_v<T>;

#define __BAN_TRAITS_MAKE_UNSIGNED_CV(__type)																	\
	template<> struct make_unsigned<__type>					{ using type = unsigned __type; };					\
	template<> struct make_unsigned<const __type>			{ using type = unsigned const __type; };			\
	template<> struct make_unsigned<volatile __type>		{ using type = unsigned volatile __type; };			\
	template<> struct make_unsigned<const volatile __type>	{ using type = unsigned const volatile __type; };

	template<typename T> requires is_arithmetic_v<T> struct make_unsigned { using type = T; };
	__BAN_TRAITS_MAKE_UNSIGNED_CV(char)
	__BAN_TRAITS_MAKE_UNSIGNED_CV(short)
	__BAN_TRAITS_MAKE_UNSIGNED_CV(int)
	__BAN_TRAITS_MAKE_UNSIGNED_CV(long)
	__BAN_TRAITS_MAKE_UNSIGNED_CV(long long)
	template<typename T> using make_unsigned_t = typename make_unsigned<T>::type;
#undef __BAN_TRAITS_MAKE_UNSIGNED_CV

#define __BAN_TRAITS_MAKE_SIGNED_CV(__type)																		\
	template<> struct make_signed<unsigned __type>					{ using type = __type; };					\
	template<> struct make_signed<unsigned const __type>			{ using type = const __type; };				\
	template<> struct make_signed<unsigned volatile __type>			{ using type = volatile __type; };			\
	template<> struct make_signed<unsigned const volatile __type>	{ using type = const volatile __type; };

	template<typename T> requires is_arithmetic_v<T> struct make_signed { using type = T; };
	__BAN_TRAITS_MAKE_SIGNED_CV(char)
	__BAN_TRAITS_MAKE_SIGNED_CV(short)
	__BAN_TRAITS_MAKE_SIGNED_CV(int)
	__BAN_TRAITS_MAKE_SIGNED_CV(long)
	__BAN_TRAITS_MAKE_SIGNED_CV(long long)
	template<typename T> using make_signed_t = typename make_signed<T>::type;
#undef __BAN_TRAITS_MAKE_SIGNED_CV

	template<typename T> struct less	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs < rhs; } };
	template<typename T> struct equal	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs == rhs; } };
	template<typename T> struct greater	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs > rhs; } };

}
