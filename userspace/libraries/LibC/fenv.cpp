#include <fenv.h>

static uint16_t read_fpu_control_word()
{
	uint16_t control;
	asm volatile("fstcw %0" : "=m"(control));
	return control;
}

static uint16_t read_fpu_status_word()
{
	uint16_t status;
	asm volatile("fstsw %0" : "=m"(status));
	return status;
}

static uint32_t read_mxcsr()
{
	uint32_t mxcsr;
	asm volatile("stmxcsr %0" : "=m"(mxcsr));
	return mxcsr;
}

static void write_fpu_control_word(uint16_t control)
{
	asm volatile("fldcw %0" :: "m"(control));
}

static void write_mxcsr(uint32_t mxcsr)
{
	asm volatile("ldmxcsr %0" :: "m"(mxcsr));
}

int fegetenv(fenv_t* envp)
{
	asm volatile("fstenv %0" : "=m"(envp->x87_fpu));
	envp->mxcsr = read_mxcsr();
	return 0;
}

int fesetenv(const fenv_t* envp)
{
	if (envp == FE_DFL_ENV)
	{
		asm volatile("finit");
		write_mxcsr(0x1F80);
		return 0;
	}

	asm volatile("fldenv %0" :: "m"(envp->x87_fpu));
	write_mxcsr(envp->mxcsr);
	return 0;
}

int fegetround(void)
{
	return (read_fpu_control_word() >> 10) & 0x03;
}

int fesetround(int round)
{
	uint16_t control = read_fpu_control_word();
	control &= ~(3 << 10);
	control |= round << 10;
	write_fpu_control_word(control);

	uint32_t mxcsr = read_mxcsr();
	mxcsr &= ~(3 << 13);
	mxcsr |= round << 13;
	write_mxcsr(mxcsr);

	return 0;
}

int feclearexcept(int excepts)
{
	excepts &= FE_ALL_EXCEPT;

	fenv_t temp_env;
	fegetenv(&temp_env);
	temp_env.x87_fpu.status &= ~(excepts | (1 << 7));
	temp_env.mxcsr &= ~excepts;
	fesetenv(&temp_env);
	return 0;
}

int fetestexcept(int excepts)
{
	excepts &= FE_ALL_EXCEPT;

	const uint16_t status = read_fpu_status_word();
	const uint32_t mxcsr = read_mxcsr();
	return (status | mxcsr) & excepts;
}

int feholdexcept(fenv_t*);

int feraiseexcept(int excepts)
{
	excepts &= FE_ALL_EXCEPT;

	fenv_t temp_env;
	fegetenv(&temp_env);
	temp_env.x87_fpu.status |= excepts;
	fesetenv(&temp_env);
	asm volatile("fwait");
	return 0;
}

int fegetexceptflag(fexcept_t* flagp, int excepts)
{
	*flagp = fetestexcept(excepts);
	return 0;
}

int fesetexceptflag(const fexcept_t* flagp, int excepts)
{
	excepts &= FE_ALL_EXCEPT;

	fenv_t temp_env;
	fegetenv(&temp_env);
	temp_env.x87_fpu.status &= ~(FE_ALL_EXCEPT | (1 << 7));
	temp_env.x87_fpu.status |= *flagp & excepts;
	fesetenv(&temp_env);
	return 0;
}

int feupdateenv(const fenv_t* envp)
{
	int excepts = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(envp);
	feraiseexcept(excepts);
	return 0;
}
