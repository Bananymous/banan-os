#include "Image.h"

BAN::ErrorOr<BAN::UniqPtr<Image>> load_netbpm(const void* mmap_addr, size_t size);
