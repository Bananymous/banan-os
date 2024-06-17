#include <BAN/ByteSpan.h>

#include <LibImage/Image.h>

namespace LibImage
{

	bool probe_png(BAN::ConstByteSpan);
	BAN::ErrorOr<BAN::UniqPtr<Image>> load_png(BAN::ConstByteSpan);

}
