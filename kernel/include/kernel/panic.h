#pragma once

namespace Kernel
{

	__attribute__((__noreturn__))
	void panic(const char* message);

}