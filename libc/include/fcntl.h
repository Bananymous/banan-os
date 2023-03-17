#pragma once

#define O_RDONLY	(1 << 0)
#define O_WRONLY	(1 << 1)
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_ACCMODE	(O_RDONLY | O_WRONLY)
#define O_EXCL		(1 << 2)
#define O_CREAT		(1 << 3)
