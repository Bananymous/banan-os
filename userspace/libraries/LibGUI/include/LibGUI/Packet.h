#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/ByteSpan.h>

#include <LibInput/KeyEvent.h>
#include <LibInput/MouseEvent.h>

#include <sys/banan-os.h>
#include <sys/socket.h>

#define FOR_EACH_0(macro)
#define FOR_EACH_2(macro, type, name)      macro(type, name)
#define FOR_EACH_4(macro, type, name, ...) macro(type, name) FOR_EACH_2(macro, __VA_ARGS__)
#define FOR_EACH_6(macro, type, name, ...) macro(type, name) FOR_EACH_4(macro, __VA_ARGS__)
#define FOR_EACH_8(macro, type, name, ...) macro(type, name) FOR_EACH_6(macro, __VA_ARGS__)

#define CONCATENATE_2(arg1, arg2) arg1 ## arg2
#define CONCATENATE_1(arg1, arg2) CONCATENATE_2(arg1, arg2)
#define CONCATENATE(arg1, arg2)   CONCATENATE_1(arg1, arg2)

#define FOR_EACH_NARG(...) FOR_EACH_NARG_(__VA_ARGS__ __VA_OPT__(,) FOR_EACH_RSEQ_N())
#define FOR_EACH_NARG_(...) FOR_EACH_ARG_N(__VA_ARGS__)
#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define FOR_EACH_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0

#define FOR_EACH_(N, what, ...) CONCATENATE(FOR_EACH_, N)(what __VA_OPT__(,) __VA_ARGS__)
#define FOR_EACH(what, ...) FOR_EACH_(FOR_EACH_NARG(__VA_ARGS__), what __VA_OPT__(,) __VA_ARGS__)

#define FIELD_DECL(type, name) type name;
#define ADD_SERIALIZED_SIZE(type, name) serialized_size += Serialize::serialized_size_impl<type>(this->name);
#define SEND_SERIALIZED(type, name) TRY(Serialize::send_serialized_impl<type>(socket, this->name));
#define DESERIALIZE(type, name) value.name = TRY(Serialize::deserialize_impl<type>(buffer));

#define DEFINE_PACKET_EXTRA(name, extra, ...) \
	struct name \
	{ \
		static constexpr PacketType type = PacketType::name; \
		static constexpr uint32_t type_u32 = static_cast<uint32_t>(type); \
 \
		extra; \
 \
		FOR_EACH(FIELD_DECL, __VA_ARGS__) \
 \
		size_t serialized_size() \
		{ \
			size_t serialized_size = Serialize::serialized_size_impl<uint32_t>(type_u32); \
			FOR_EACH(ADD_SERIALIZED_SIZE, __VA_ARGS__) \
			return serialized_size; \
		} \
 \
		BAN::ErrorOr<void> send_serialized(int socket) \
		{ \
			const uint32_t serialized_size = this->serialized_size(); \
			TRY(Serialize::send_serialized_impl<uint32_t>(socket, serialized_size)); \
			TRY(Serialize::send_serialized_impl<uint32_t>(socket, type_u32)); \
			FOR_EACH(SEND_SERIALIZED, __VA_ARGS__) \
			return {}; \
		} \
 \
		static BAN::ErrorOr<name> deserialize(BAN::ConstByteSpan buffer) \
		{ \
			const uint32_t type_u32 = TRY(Serialize::deserialize_impl<uint32_t>(buffer)); \
			if (type_u32 != name::type_u32) \
				return BAN::Error::from_errno(EINVAL); \
			name value; \
			FOR_EACH(DESERIALIZE, __VA_ARGS__) \
			return value; \
		} \
	}

#define DEFINE_PACKET(name, ...) DEFINE_PACKET_EXTRA(name, , __VA_ARGS__)

namespace LibGUI
{

	static constexpr BAN::StringView s_window_server_socket = "/tmp/window-server.socket"_sv;

	namespace Serialize
	{

		inline BAN::ErrorOr<void> send_raw_data(int socket, BAN::ConstByteSpan data)
		{
			size_t send_done = 0;
			while (send_done < data.size())
			{
				const ssize_t nsend = ::send(socket, data.data() + send_done, data.size() - send_done, 0);
				if (nsend < 0)
					return BAN::Error::from_errno(errno);
				if (nsend == 0)
					return BAN::Error::from_errno(ECONNRESET);
				send_done += nsend;
			}
			return {};
		}

		template<typename T> requires BAN::is_pod_v<T>
		inline size_t serialized_size_impl(const T&)
		{
			return sizeof(T);
		}

		template<typename T> requires BAN::is_pod_v<T>
		inline BAN::ErrorOr<void> send_serialized_impl(int socket, const T& value)
		{
			TRY(send_raw_data(socket, BAN::ConstByteSpan::from(value)));
			return {};
		}

		template<typename T> requires BAN::is_pod_v<T>
		inline BAN::ErrorOr<T> deserialize_impl(BAN::ConstByteSpan& buffer)
		{
			if (buffer.size() < sizeof(T))
				return BAN::Error::from_errno(ENOBUFS);
			const T value = buffer.as<const T>();
			buffer = buffer.slice(sizeof(T));
			return value;
		}

		template<typename T> requires BAN::is_same_v<T, BAN::String>
		inline size_t serialized_size_impl(const T& value)
		{
			return sizeof(uint32_t) + value.size();
		}

		template<typename T> requires BAN::is_same_v<T, BAN::String>
		inline BAN::ErrorOr<void> send_serialized_impl(int socket, const T& value)
		{
			const uint32_t value_size = value.size();
			TRY(send_raw_data(socket, BAN::ConstByteSpan::from(value_size)));
			auto* u8_data = reinterpret_cast<const uint8_t*>(value.data());
			TRY(send_raw_data(socket, BAN::ConstByteSpan(u8_data, value.size())));
			return {};
		}

		template<typename T> requires BAN::is_same_v<T, BAN::String>
		inline BAN::ErrorOr<T> deserialize_impl(BAN::ConstByteSpan& buffer)
		{
			if (buffer.size() < sizeof(uint32_t))
				return BAN::Error::from_errno(ENOBUFS);
			const uint32_t string_len = buffer.as<const uint32_t>();
			buffer = buffer.slice(sizeof(uint32_t));

			if (buffer.size() < string_len)
				return BAN::Error::from_errno(ENOBUFS);

			BAN::String string;
			TRY(string.resize(string_len));
			memcpy(string.data(), buffer.data(), string_len);
			buffer = buffer.slice(string_len);

			return string;
		}

	}

	enum class PacketType : uint32_t
	{
		WindowCreate,
		WindowCreateResponse,
		WindowInvalidate,
		WindowSetPosition,
		WindowSetAttributes,
		WindowSetMouseCapture,
		WindowSetSize,
		WindowSetMinSize,
		WindowSetMaxSize,
		WindowSetFullscreen,
		WindowSetTitle,

		DestroyWindowEvent,
		CloseWindowEvent,
		ResizeWindowEvent,
		WindowShownEvent,
		KeyEvent,
		MouseButtonEvent,
		MouseMoveEvent,
		MouseScrollEvent,
	};

	namespace WindowPacket
	{

		struct Attributes
		{
			bool title_bar;
			bool movable;
			bool focusable;
			bool rounded_corners;
			bool alpha_channel;
			bool resizable;
			bool shown;
		};

		DEFINE_PACKET(
			WindowCreate,
			uint32_t, width,
			uint32_t, height,
			Attributes, attributes,
			BAN::String, title
		);

		DEFINE_PACKET(
			WindowInvalidate,
			uint32_t, x,
			uint32_t, y,
			uint32_t, width,
			uint32_t, height
		);

		DEFINE_PACKET(
			WindowSetPosition,
			int32_t, x,
			int32_t, y
		);

		DEFINE_PACKET(
			WindowSetAttributes,
			Attributes, attributes
		);

		DEFINE_PACKET(
			WindowSetMouseCapture,
			bool, captured
		);

		DEFINE_PACKET(
			WindowSetSize,
			uint32_t, width,
			uint32_t, height
		);

		DEFINE_PACKET(
			WindowSetMinSize,
			uint32_t, width,
			uint32_t, height
		);

		DEFINE_PACKET(
			WindowSetMaxSize,
			uint32_t, width,
			uint32_t, height
		);

		DEFINE_PACKET(
			WindowSetFullscreen,
			bool, fullscreen
		);

		DEFINE_PACKET(
			WindowSetTitle,
			BAN::String, title
		);

	}

	namespace EventPacket
	{

		DEFINE_PACKET(
			DestroyWindowEvent
		);

		DEFINE_PACKET(
			CloseWindowEvent
		);

		DEFINE_PACKET(
			ResizeWindowEvent,
			uint32_t, width,
			uint32_t, height,
			long, smo_key
		);

		DEFINE_PACKET_EXTRA(
			WindowShownEvent,
			struct event_t {
				bool shown;
			},
			event_t, event
		);

		DEFINE_PACKET_EXTRA(
			KeyEvent,
			using event_t = LibInput::KeyEvent,
			event_t, event
		);

		DEFINE_PACKET_EXTRA(
			MouseButtonEvent,
			struct event_t {
				LibInput::MouseButton button;
				bool pressed;
				int32_t x;
				int32_t y;
			},
			event_t, event
		);

		DEFINE_PACKET_EXTRA(
			MouseMoveEvent,
			struct event_t {
				int32_t x;
				int32_t y;
			},
			event_t, event
		);

		DEFINE_PACKET_EXTRA(
			MouseScrollEvent,
			struct event_t {
				int32_t scroll;
			},
			event_t, event
		);

	}

}
