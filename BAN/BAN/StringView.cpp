#include <BAN/String.h>
#include <BAN/StringView.h>

namespace BAN
{

	StringView::StringView(const String& other)
		: StringView(other.data(), other.size())
	{ }

}
