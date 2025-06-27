#pragma once

#include <BAN/Span.h>
#include <BAN/StringView.h>

#include <stddef.h>

namespace LibGUI
{

	class MessageBox
	{
	public:
		static BAN::ErrorOr<void> create(BAN::StringView message, BAN::StringView title);
		static BAN::ErrorOr<size_t> create(BAN::StringView message, BAN::StringView title, BAN::Span<BAN::StringView> buttons);
	};

}
