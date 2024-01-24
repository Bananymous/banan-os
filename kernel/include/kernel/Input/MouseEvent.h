#pragma once

#include <stdint.h>

namespace Kernel::Input
{

	enum class MouseButton
	{
		Left, Right, Middle, Extra1, Extra2
	};

	struct MouseButtonEvent
	{
		MouseButton button;
		bool pressed;
	};

	struct MouseMoveEvent
	{
		int32_t rel_x;
		int32_t rel_y;
	};

	struct MouseScrollEvent
	{
		int32_t scroll;
	};

	enum class MouseEventType
	{
		MouseButtonEvent,
		MouseMoveEvent,
		MouseScrollEvent,
	};

	struct MouseEvent
	{
		MouseEventType type;
		union
		{
			MouseButtonEvent button_event;
			MouseMoveEvent move_event;
			MouseScrollEvent scroll_event;
		};
	};

}
