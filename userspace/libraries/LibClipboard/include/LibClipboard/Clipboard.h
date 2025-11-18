#pragma once

#include <BAN/Span.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

namespace LibClipboard
{

	static constexpr BAN::StringView s_clipboard_server_socket = "/tmp/clipboard-server.socket"_sv;

	class Clipboard
	{
	public:
		enum class DataType : uint32_t
		{
			None,
			Text,
			__get = UINT32_MAX,
		};

		struct Info
		{
			DataType type = DataType::None;
			BAN::Vector<uint8_t> data;
		};

	public:
		static BAN::ErrorOr<Info> get_clipboard();
		static BAN::ErrorOr<void> set_clipboard(DataType type, BAN::Span<const uint8_t> data);

		static BAN::ErrorOr<BAN::String> get_clipboard_text();
		static BAN::ErrorOr<void> set_clipboard_text(BAN::StringView string);
	};

}
