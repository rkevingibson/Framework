#include "Input.h"

using namespace rkg;

bool Input::MouseButton[3]{ false, false, false };
bool Input::MouseButtonPressed[3]{ false, false,false };
bool Input::MouseButtonReleased[3]{ false, false, false };
Vec2 Input::MousePosition{ 0,0 };
Vec2 Input::MouseWheelDelta{ 0,0 };
Vec2 Input::ScreenSize{ 0,0 };