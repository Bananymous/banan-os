#pragma once

#include <stdint.h>

namespace LibInput
{

	enum JoystickButton
	{
		JSB_DPAD_UP,
		JSB_DPAD_DOWN,
		JSB_DPAD_LEFT,
		JSB_DPAD_RIGHT,

		JSB_FACE_UP,
		JSB_FACE_DOWN,
		JSB_FACE_LEFT,
		JSB_FACE_RIGHT,

		JSB_STICK_LEFT,
		JSB_STICK_RIGHT,

		JSB_SHOULDER_LEFT,
		JSB_SHOULDER_RIGHT,

		JSB_MENU,
		JSB_START,
		JSB_SELECT,

		JSB_COUNT,
	};

	enum JoystickAxis
	{
		JSA_STICK_LEFT_X,
		JSA_STICK_LEFT_Y,
		JSA_STICK_RIGHT_X,
		JSA_STICK_RIGHT_Y,

		JSA_TRIGGER_LEFT,
		JSA_TRIGGER_RIGHT,

		JSA_COUNT,
	};

	// TODO: not used but exists if we ever make controller
	//       support generating events instead of being polled
	struct JoystickEvent
	{

	};

	struct JoystickState
	{
		// axis are mapped to range [-32767, +32767]
		int16_t axis[JSA_COUNT];
		bool buttons[JSB_COUNT];
	};

}
