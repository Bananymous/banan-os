#pragma once

namespace BAN
{

	template<typename T>
	T min(T a, T b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	T max(T a, T b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	T clamp(T x, T min, T max)
	{
		return x < min ? min : x > max ? max : x;
	}

}