#include <BAN/Math.h>
#include <BAN/Traits.h>

#include <complex.h>
#include <math.h>

template<BAN::floating_point T> struct _complex_t;
template<> struct _complex_t<float>       { using type = __complex__ float; };
template<> struct _complex_t<double>      { using type = __complex__ double; };
template<> struct _complex_t<long double> { using type = __complex__ long double; };

template<BAN::floating_point T>
using _complex = _complex_t<T>::type;

template<BAN::floating_point T>
static constexpr T _cabs(_complex<T> z)
{
	return BAN::Math::sqrt(creal(z) * creal(z) + cimag(z) * cimag(z));
}

template<BAN::floating_point T>
static constexpr T _carg(_complex<T> z)
{
	return BAN::Math::atan2(cimag(z), creal(z));
}

template<BAN::floating_point T>
static constexpr _complex<T> _cproj(_complex<T> z)
{
	if (!isfinite(creal(z)) || !isfinite(cimag(z)))
		return INFINITY + I * copysign(0.0, cimag(z));
	return z;
}

template<BAN::floating_point T>
static constexpr _complex<T> _conj(_complex<T> z)
{
	cimag(z) = -cimag(z);
	return z;
}

template<BAN::floating_point T>
static constexpr _complex<T> _cexp(_complex<T> z)
{
	T sin, cos;
	BAN::Math::sincos(cimag(z), sin, cos);
	return BAN::Math::exp(creal(z)) * (cos + sin * I);
}

template<BAN::floating_point T>
static constexpr _complex<T> _clog(_complex<T> z)
{
	return BAN::Math::log(_cabs<T>(z)) + I * _carg<T>(z);
}

template<BAN::floating_point T>
static constexpr _complex<T> _csqrt(_complex<T> z)
{
	return BAN::Math::sqrt(_cabs<T>(z)) * _cexp<T>(I * _carg<T>(z) / 2);
}

template<BAN::floating_point T>
static constexpr _complex<T> _csin(_complex<T> z)
{
	return (_cexp<T>(z * I) - _cexp<T>(-z * I)) / 2i;
}

template<BAN::floating_point T>
static constexpr _complex<T> _ccos(_complex<T> z)
{
	return (_cexp<T>(z * I) + _cexp<T>(-z * I)) / 2i;
}

template<BAN::floating_point T>
static constexpr _complex<T> _ctan(_complex<T> z)
{
	const _complex<T> exp_pos = _cexp<T>(+I * z);
	const _complex<T> exp_neg = _cexp<T>(-I * z);
	return -I * (exp_pos - exp_neg) / (exp_pos + exp_neg);
}

template<BAN::floating_point T>
static constexpr _complex<T> _cpow(_complex<T> x, _complex<T> y)
{
	const T ln_r  = BAN::Math::log(_cabs<T>(x));
	const T theta = _carg<T>(x);
	const T a = creal(y);
	const T b = cimag(y);
	return BAN::Math::exp(a * ln_r - b * theta) * _cexp<T>(I * (a * theta + b * ln_r));
}

template<BAN::floating_point T>
static constexpr _complex<T> _casin(_complex<T> z)
{
	return -I * _clog<T>(_csqrt<T>(1 - z * z) + I * z);
}

template<BAN::floating_point T>
static constexpr _complex<T> _cacos(_complex<T> z)
{
	return -I * _clog<T>(I * _csqrt<T>(1 - z * z) + z);
}

template<BAN::floating_point T>
static constexpr _complex<T> _catan(_complex<T> z)
{
	return -I / 2 * _clog<T>((I - z) / (I + z));
}

template<BAN::floating_point T>
static constexpr _complex<T> _csinh(_complex<T> z)
{
	return (_cexp<T>(z) - _cexp<T>(-z)) / 2;
}

template<BAN::floating_point T>
static constexpr _complex<T> _ccosh(_complex<T> z)
{
	return (_cexp<T>(z) + _cexp<T>(-z)) / 2;
}

template<BAN::floating_point T>
static constexpr _complex<T> _ctanh(_complex<T> z)
{
	const _complex<T> exp2x = _cexp<T>(2 * z);
	return (exp2x - 1) / (exp2x + 1);
}

template<BAN::floating_point T>
static constexpr _complex<T> _casinh(_complex<T> z)
{
	return _clog<T>(z + _csqrt<T>(z * z + 1));
}

template<BAN::floating_point T>
static constexpr _complex<T> _cacosh(_complex<T> z)
{
	return _clog<T>(z + _csqrt<T>(z * z - 1));
}

template<BAN::floating_point T>
static constexpr _complex<T> _catanh(_complex<T> z)
{
	return _clog<T>((1 + z) / (1 - z)) / 2;
}


#define COMPLEX_FUNCS(func) \
	float func##f(float complex a) { return _##func<float>(a); } \
	double func(double complex a) { return _##func<double>(a); } \
	long double func##l(long double complex a) { return _##func<long double>(a); }

COMPLEX_FUNCS(cabs)
COMPLEX_FUNCS(carg)

#undef COMPLEX_FUNCS


#define COMPLEX_FUNCS(func) \
	float complex func##f(float complex a) { return _##func<float>(a); } \
	double complex func(double complex a) { return _##func<double>(a); } \
	long double complex func##l(long double complex a) { return _##func<long double>(a); }

COMPLEX_FUNCS(cproj)
COMPLEX_FUNCS(conj)
COMPLEX_FUNCS(cexp)
COMPLEX_FUNCS(ctan)
COMPLEX_FUNCS(clog)
COMPLEX_FUNCS(csin)
COMPLEX_FUNCS(ccos)
COMPLEX_FUNCS(csqrt)
COMPLEX_FUNCS(csinh)
COMPLEX_FUNCS(ccosh)
COMPLEX_FUNCS(ctanh)
COMPLEX_FUNCS(cacos)
COMPLEX_FUNCS(casin)
COMPLEX_FUNCS(catan)
COMPLEX_FUNCS(cacosh)
COMPLEX_FUNCS(casinh)
COMPLEX_FUNCS(catanh)

#undef COMPLEX_FUNCS


#define COMPLEX_FUNCS(func) \
	float complex func##f(float complex a, float complex b) { return _##func<float>(a, b); } \
	double complex func(double complex a, double complex b) { return _##func<double>(a, b); } \
	long double complex func##l(long double complex a, long double complex b) { return _##func<long double>(a, b); }

COMPLEX_FUNCS(cpow)

#undef COMPLEX_FUNCS
