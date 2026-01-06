#include <BAN/Debug.h>

#include <errno.h>
#include <sys/shm.h>

#define TODO_FUNC(type, name, ...) type name(__VA_ARGS__) { dwarnln("TODO: " #name); errno = ENOTSUP; return (type)-1; }

TODO_FUNC(void*, shmat, int, const void*, int)
TODO_FUNC(int,   shmctl, int, int, struct shmid_ds*)
TODO_FUNC(int,   shmdt, const void*)
TODO_FUNC(int,   shmget, key_t, size_t, int)
