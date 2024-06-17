#include <BAN/ByteSpan.h>

#include <LibImage/Image.h>

namespace LibImage
{

	bool probe_netbpm(BAN::ConstByteSpan);
	BAN::ErrorOr<BAN::UniqPtr<Image>> load_netbpm(BAN::ConstByteSpan);

}
