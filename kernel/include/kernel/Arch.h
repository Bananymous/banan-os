#pragma once

#define ARCH(arch) (__arch == arch)

#if !defined(__arch) || (__arch != x86_64 && __arch != i386)
	#error "Unsupported architecture"
#endif