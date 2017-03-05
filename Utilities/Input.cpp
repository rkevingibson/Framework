#include "Input.h"
#include "Utilities.h"



#include <vector>
#include <utility>

namespace rkg 
{
bool Input::MouseButton[3]{ false, false, false };
bool Input::MouseButtonPressed[3]{ false, false,false };
bool Input::MouseButtonReleased[3]{ false, false, false };
Vec2 Input::MousePosition{ 0,0 };
Vec2 Input::MouseWheelDelta{ 0,0 };
Vec2 Input::ScreenSize{ 0,0 };

namespace {
//Store 3 bits for every key-lower bit is current status (pressed or released)
//bit 1 is set if key was pressed this frame. Bit 2 if it was released.
uint8_t key_status[static_cast<int>(Input::Keyname::NUM_KEYNAMES)];


}


void Input::NewFrame()
{
	for (int i = 0; i < RKG_ARRAY_LENGTH(key_status); i++) {
		key_status[i] = key_status[i] & 0b1;
	}
}

void Input::SetKeyStatus(Keyname key, KeyAction action)
{
	Expects(key < Keyname::NUM_KEYNAMES);
	if (action == KeyAction::PRESSED) {
		key_status[static_cast<int>(key)] |= 0b011;
	}
	else if (action == KeyAction::RELEASED) {
		key_status[static_cast<int>(key)] |= 0b100;
		key_status[static_cast<int>(key)] &= 0b110;
	}
}


bool Input::GetKey(Keyname key)
{
	Expects(key < Keyname::NUM_KEYNAMES);
	return (key_status[static_cast<int>(key)] & 0b001);
}

bool Input::GetKeyDown(Keyname key)
{
	Expects(key < Keyname::NUM_KEYNAMES);
	return ((key_status[static_cast<int>(key)] & 0b010) == 0b010);
}

bool Input::GetKeyUp(Keyname key)
{
	Expects(key < Keyname::NUM_KEYNAMES);
	return ((key_status[static_cast<int>(key)] & 0b100) == 0b100);
}



namespace
{
struct ResizeCallbackStruct {
	Input::ResizeCallback fn;
	void* data{ nullptr };
};

std::vector<ResizeCallbackStruct> callbacks;

}


void Input::RegisterResizeCallback(ResizeCallback callback, void * user_data)
{
	ResizeCallbackStruct s;
	s.fn = callback;
	s.data = user_data;
	callbacks.emplace_back(std::move(s));
}

void Input::ResizeScreen(int w, int h)
{
	rkg::Input::ScreenSize = { (float)w, (float)h };
	for (auto& c : callbacks) {
		c.fn(w, h, c.data);
	}
}

}