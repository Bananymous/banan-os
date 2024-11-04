#include <BAN/Math.h>

#include <math.h>
#include <stddef.h>

#define BAN_FUNC1(func) \
	float func##f(float a) { return BAN::Math::func<float>(a); } \
	double func(double a) { return BAN::Math::func<double>(a); } \
	long double func##l(long double a) { return BAN::Math::func<long double>(a); }

#define BAN_FUNC2(func) \
	float func##f(float a, float b) { return BAN::Math::func<float>(a, b); } \
	double func(double a, double b) { return BAN::Math::func<double>(a, b); } \
	long double func##l(long double a, long double b) { return BAN::Math::func<long double>(a, b); }

#define BAN_FUNC2_PTR(func) \
	float func##f(float a, float* b) { return BAN::Math::func<float>(a, b); } \
	double func(double a, double* b) { return BAN::Math::func<double>(a, b); } \
	long double func##l(long double a, long double* b) { return BAN::Math::func<long double>(a, b); }

#define BAN_FUNC2_TYPE(func, type) \
	float func##f(float a, type b) { return BAN::Math::func<float>(a, b); } \
	double func(double a, type b) { return BAN::Math::func<double>(a, b); } \
	long double func##l(long double a, type b) { return BAN::Math::func<long double>(a, b); }

#define FUNC_EXPR1(func, ...) \
	float func##f(float a) { return __VA_ARGS__; } \
	double func(double a) { return __VA_ARGS__; } \
	long double func##l(long double a) { return __VA_ARGS__; }

#define FUNC_EXPR2(func, ...) \
	float func##f(float a, float b) { return __VA_ARGS__; } \
	double func(double a, double b) { return __VA_ARGS__; } \
	long double func##l(long double a, long double b) { return __VA_ARGS__; }

#define FUNC_EXPR3(func, ...) \
	float func##f(float a, float b, float c) { return __VA_ARGS__; } \
	double func(double a, double b, double c) { return __VA_ARGS__; } \
	long double func##l(long double a, long double b, long double c) { return __VA_ARGS__; }

#define FUNC_EXPR1_RET(func, ret, ...) \
	ret func##f(float a) { return __VA_ARGS__; } \
	ret func(double a) { return __VA_ARGS__; } \
	ret func##l(long double a) { return __VA_ARGS__; }

#define FUNC_EXPR2_TYPE(func, type, ...) \
	float func##f(float a, type b) { return __VA_ARGS__; } \
	double func(double a, type b) { return __VA_ARGS__; } \
	long double func##l(long double a, type b) { return __VA_ARGS__; }

template<BAN::floating_point T> struct float_underlying;
template<> struct float_underlying<float>       { using type = uint32_t; };
template<> struct float_underlying<double>      { using type = uint64_t; };
template<> struct float_underlying<long double> { using type = uint64_t; };

template<BAN::integral T, size_t mantissa_bits, size_t exponent_bits, bool integral>
struct __FloatDecompose;

template<BAN::integral T, size_t mantissa_bits, size_t exponent_bits>
struct __FloatDecompose<T, mantissa_bits, exponent_bits, true>
{
	T mantissa : mantissa_bits;
	T          : 1;
	T exponent : exponent_bits;
	T sign     : 1;
};

template<BAN::integral T, size_t mantissa_bits, size_t exponent_bits>
struct __FloatDecompose<T, mantissa_bits, exponent_bits, false>
{
	T mantissa : mantissa_bits;
	T exponent : exponent_bits;
	T sign     : 1;
};

template<BAN::floating_point T>
struct FloatDecompose
{
	using value_type = float_underlying<T>::type;

	static constexpr size_t mantissa_bits
		= BAN::is_same_v<T, float>       ?  FLT_MANT_DIG - 1
		: BAN::is_same_v<T, double>      ?  DBL_MANT_DIG - 1
		: BAN::is_same_v<T, long double> ? LDBL_MANT_DIG - 1
		: 0;

	static constexpr size_t exponent_bits
		= BAN::is_same_v<T, float>       ? 8
		: BAN::is_same_v<T, double>      ? 11
		: BAN::is_same_v<T, long double> ? 15
		: 0;

	static constexpr value_type mantissa_max
		= (static_cast<value_type>(1) << mantissa_bits) - 1;

	static constexpr value_type exponent_max
		= (static_cast<value_type>(1) << exponent_bits) - 1;

	FloatDecompose(T x)
		: raw(x)
	{}

	union
	{
		T raw;
		__FloatDecompose<
			value_type,
			mantissa_bits,
			exponent_bits,
			BAN::is_same_v<T, long double>
		> bits;
	};
};

template<BAN::floating_point T1, BAN::floating_point T2>
static T1 nextafter_impl(T1 x, T2 y)
{
	if (isnan(x) || isnan(y))
		return BAN::numeric_limits<T1>::quiet_NaN();

	if (!isfinite(x) || x == y)
		return x;

	FloatDecompose<T1> decompose(x);

	// at zero
	if ((T2)x == (T2)0.0)
	{
		decompose.bits.mantissa = 1;
		decompose.bits.sign = (x > y);
		return decompose.raw;
	}

	// away from zero
	if ((x > y) == decompose.bits.sign)
	{
		decompose.bits.mantissa++;
		if (decompose.bits.mantissa == 0)
		{
			decompose.bits.exponent++;
			if (decompose.bits.exponent == decompose.exponent_max)
				decompose.bits.mantissa = 0;
		}
		return decompose.raw;
	}

	// towards zero
	decompose.bits.mantissa--;
	if (decompose.bits.mantissa == decompose.mantissa_max)
	{
		if (decompose.bits.exponent == 0)
			return 0.0;
		decompose.bits.exponent--;
	}

	return decompose.raw;
}

static long double tgamma_impl(long double x)
{
	constexpr long double pi = BAN::numbers::pi_v<long double>;

	if (x == 0.0L)
		return BAN::numeric_limits<long double>::infinity();

	// reflection formula
	if (x < 0.5L)
		return pi / (BAN::Math::sin(pi * x) * tgamma_impl(1.0L - x));
	x -= 1.0L;

	// Lanczos approximation

	constexpr long double g = 8.0L;
	constexpr long double p[] {
		 0.9999999999999999298e+0L,
		 1.9753739023578852322e+3L,
		-4.3973823927922428918e+3L,
		 3.4626328459862717019e+3L,
		-1.1569851431631167820e+3L,
		 1.5453815050252775060e+2L,
		-6.2536716123689161798e+0L,
		 3.4642762454736807441e-2L,
		-7.4776171974442977377e-7L,
		 6.3041253821852264261e-8L,
		-2.7405717035683877489e-8L,
		 4.0486948817567609101e-9L
	};
	constexpr long double sqrt_2pi = 2.5066282746310005024L;

	long double A = p[0];
	for (size_t i = 1; i < sizeof(p) / sizeof(*p); i++)
		A += p[i] / (x + i);

	const long double t = x + g + 0.5L;
	return sqrt_2pi * BAN::Math::pow(t, x + 0.5L) * BAN::Math::exp(-t) * A;
}

static long double erf_impl(long double x)
{
	long double sum = 0.0L;
	for (size_t n = 0; n < 100; n++)
		sum += x / (2.0L * n + 1.0L) * BAN::Math::ldexp(-x * x, n) / tgamma_impl(n + 1.0L);

	constexpr long double sqrt_pi = 1.77245385090551602729L;
	return 2.0L / sqrt_pi * sum;
}

__BEGIN_DECLS

// FIXME: add handling for nan and infinity values

BAN_FUNC1(acos)
BAN_FUNC1(acosh)
BAN_FUNC1(asin)
BAN_FUNC1(asinh)
BAN_FUNC1(atan)
BAN_FUNC2(atan2)
BAN_FUNC1(atanh)
BAN_FUNC1(cbrt)
BAN_FUNC1(ceil)
BAN_FUNC2(copysign)
BAN_FUNC1(cos)
BAN_FUNC1(cosh)
FUNC_EXPR1(erf, erf_impl(a))
FUNC_EXPR1(erfc, 1.0 - erf_impl(a))
BAN_FUNC1(exp)
BAN_FUNC1(exp2)
FUNC_EXPR1(expm1, ({ a -= 1.0; BAN::Math::exp(a); }))
FUNC_EXPR1(fabs, BAN::Math::abs(a))
FUNC_EXPR2(fdim, a > b ? a - b : 0.0)
BAN_FUNC1(floor)
FUNC_EXPR3(fma, (a * b) + c)
FUNC_EXPR2(fmax, a < b ? b : a)
FUNC_EXPR2(fmin, a < b ? a : b)
BAN_FUNC2(fmod)
BAN_FUNC2_TYPE(frexp, int*)
BAN_FUNC2(hypot)
FUNC_EXPR1_RET(ilogb, int, BAN::Math::logb(a))
// j0, j1, jn
BAN_FUNC2_TYPE(ldexp, int)
FUNC_EXPR1(lgamma, BAN::Math::log(BAN::Math::abs(tgamma_impl(a))))
FUNC_EXPR1_RET(llrint,  long long, BAN::Math::rint(a))
FUNC_EXPR1_RET(llround, long long, BAN::Math::round(a))
BAN_FUNC1(log)
BAN_FUNC1(log10)
FUNC_EXPR1(log1p, ({ a += 1.0; BAN::Math::log(a); }));
BAN_FUNC1(log2)
BAN_FUNC1(logb)
FUNC_EXPR1_RET(lrint,  long, BAN::Math::rint(a))
FUNC_EXPR1_RET(lround, long, BAN::Math::round(a))
BAN_FUNC2_PTR(modf)
FUNC_EXPR1(nearbyint, BAN::Math::rint(a))
FUNC_EXPR2(nextafter, nextafter_impl(a, b))
FUNC_EXPR2_TYPE(nexttoward, long double, nextafter_impl(a, b))
FUNC_EXPR2(pow, ({ if (isnan(a)) return a; if (isnan(b)) return b; BAN::Math::pow(a, b); }))
// remainder
// remquo
BAN_FUNC1(rint)
FUNC_EXPR1(round, ({ if (!isfinite(a)) return a; BAN::Math::round(a); }))
FUNC_EXPR2_TYPE(scalbln, long, BAN::Math::scalbn(a, b))
FUNC_EXPR2_TYPE(scalbn,  int,  BAN::Math::scalbn(a, b))
BAN_FUNC1(sin)
BAN_FUNC1(sinh)
BAN_FUNC1(sqrt)
BAN_FUNC1(tan)
BAN_FUNC1(tanh)
FUNC_EXPR1(tgamma, tgamma_impl(a))
BAN_FUNC1(trunc)
// y0, y1, yn


__END_DECLS
