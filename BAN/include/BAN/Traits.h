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

	template<typename T> struct remove_const_and_reference { using type = remove_const_t<remove_reference_t<T>>; };
	template<typename T> using remove_const_and_reference_t = typename remove_const_and_reference<T>::type;

	template<bool B, typename T = void> struct enable_if {};
	template<typename T> struct enable_if<true, T> { using type = T; };
	template<bool B, typename T = void> using enable_if_t = typename enable_if<B, T>::type;

	template<typename T, typename S> struct is_same	{ static constexpr bool value = false; };
	template<typename T> struct is_same<T, T>		{ static constexpr bool value = true; };
	template<typename T, typename S> inline constexpr bool is_same_v = is_same<T, S>::value;

	template<typename T> struct is_lvalue_reference		{ static constexpr bool value = false; };
	template<typename T> struct is_lvalue_reference<T&>	{ static constexpr bool value = true; };
	template<typename T> inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

	template<bool B, typename T> struct maybe_const { using type = T; };
	template<typename T> struct maybe_const<true, T> { using type = const T; };
	template<bool B, typename T> using maybe_const_t = typename maybe_const<B, T>::type;

	template<typename T> struct is_integral { static constexpr bool value = requires (T t, T* p, void (*f)(T)) { reinterpret_cast<T>(t); f(0); p + t; }; };
	template<typename T> inline constexpr bool is_integral_v = is_integral<T>::value;
	template<typename T> concept integral = is_integral_v<T>;

	template<typename T> struct is_pointer		{ static constexpr bool value = false; };
	template<typename T> struct is_pointer<T*>	{ static constexpr bool value = true; };
	template<typename T> inline constexpr bool is_pointer_v = is_pointer<T>::value;
	template<typename T> concept pointer = is_pointer_v<T>;

	template<typename T> struct less	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs < rhs; } };
	template<typename T> struct equal	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs == rhs; } };
	template<typename T> struct greater	{ constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs > rhs; } };

}