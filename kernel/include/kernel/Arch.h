#pragma once

#define x86_64 1
#define i686   2

#define ARCH(arch) (__arch == arch)

#if !defined(__arch) || (__arch != x86_64 && __arch != i686)
	#error "Unsupported architecture"
#endif
