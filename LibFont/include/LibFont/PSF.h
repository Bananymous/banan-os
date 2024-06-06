#pragma once

#include <LibFont/Font.h>

namespace LibFont
{

	bool is_psf1(BAN::ConstByteSpan);
	BAN::ErrorOr<Font> parse_psf1(BAN::ConstByteSpan);

	bool is_psf2(BAN::ConstByteSpan);
	BAN::ErrorOr<Font> parse_psf2(BAN::ConstByteSpan);

}
