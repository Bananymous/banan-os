#include "Image.h"

class Netbpm : public Image
{
public:
	static BAN::ErrorOr<BAN::UniqPtr<Netbpm>> create(const void* mmap_addr, size_t size);

private:
	Netbpm(uint64_t width, uint64_t height, BAN::Vector<Color>&& bitmap)
		: Image(width, height, BAN::move(bitmap))
	{ }

	friend class BAN::UniqPtr<Netbpm>;
};
