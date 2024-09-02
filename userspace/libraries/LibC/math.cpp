#include <math.h>

#define BUILTINS1(func) \
	float func##f(float a) { return __builtin_##func##f(a); } \
	double func(double a) { return __builtin_##func(a); } \
	long double func##l(long double a) { return __builtin_##func##l(a); }

#define BUILTINS2(func) \
	float func##f(float a, float b) { return __builtin_##func##f(a, b); } \
	double func(double a, double b) { return __builtin_##func(a, b); } \
	long double func##l(long double a, long double b) { return __builtin_##func##l(a, b); }

#define BUILTINS2_TYPE(func, type) \
	float func##f(float a, type b) { return __builtin_##func##f(a, b); } \
	double func(double a, type b) { return __builtin_##func(a, b); } \
	long double func##l(long double a, type b) { return __builtin_##func##l(a, b); }

__BEGIN_DECLS

#if __enable_sse
BUILTINS1(acos)
BUILTINS1(acosh)
BUILTINS1(asin)
BUILTINS1(asinh)
BUILTINS1(atan)
BUILTINS2(atan2)
BUILTINS1(atanh)
BUILTINS1(cbrt)
BUILTINS1(ceil)
BUILTINS2(copysign)
BUILTINS1(cos)
BUILTINS1(cosh)
BUILTINS1(erf)
BUILTINS1(erfc)
BUILTINS1(exp)
BUILTINS1(exp2)
BUILTINS1(expm1)
BUILTINS1(fabs)
BUILTINS2(fdim)
BUILTINS1(floor)
BUILTINS2(fmax)
BUILTINS2(fmin)
BUILTINS2(fmod)
BUILTINS2(hypot)
BUILTINS1(j0)
BUILTINS1(j1)
BUILTINS2_TYPE(ldexp, int)
BUILTINS1(lgamma)
BUILTINS1(log)
BUILTINS1(log10)
BUILTINS1(log1p)
BUILTINS1(log2)
BUILTINS1(logb)
BUILTINS1(nearbyint)
BUILTINS2(nextafter)
BUILTINS2(pow)
BUILTINS2(remainder)
BUILTINS1(rint)
BUILTINS1(round)
BUILTINS1(sin)
BUILTINS1(sinh)
BUILTINS1(sqrt)
BUILTINS1(tan)
BUILTINS1(tanh)
BUILTINS1(tgamma)
BUILTINS1(trunc)
BUILTINS1(y0)
BUILTINS1(y1)
#endif

__END_DECLS
