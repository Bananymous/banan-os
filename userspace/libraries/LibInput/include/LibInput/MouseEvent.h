#pragma once

#include <stdint.h>

namespace LibInput
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

	struct MouseMoveAbsEvent
	{
		int32_t abs_x;
		int32_t abs_y;
		int32_t min_x;
		int32_t min_y;
		int32_t max_x;
		int32_t max_y;
	};

	struct MouseScrollEvent
	{
		int32_t scroll;
	};

	enum class MouseEventType
	{
		MouseButtonEvent,
		MouseMoveEvent,
		MouseMoveAbsEvent,
		MouseScrollEvent,
	};

	struct MouseEvent
	{
		MouseEventType type;
		union
		{
			MouseButtonEvent button_event;
			MouseMoveEvent move_event;
			MouseMoveAbsEvent move_abs_event;
			MouseScrollEvent scroll_event;
		};
	};

}
