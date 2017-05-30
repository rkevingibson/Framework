#pragma once

/*
	Input system. Initially just some helpful globals. Later on, have some mapping functionality.
*/


#include "Geometry.h"

namespace rkg
{
class Input
{

public:
	static bool MouseButton[3]; //True if button is pressed. [0] = Left, [1] = Right, [2] = Middle
	static bool MouseButtonPressed[3]; //True if the button was pressed this frame.
	static bool MouseButtonReleased[3]; //True if the button was released this frame.
	static Vec2 MousePosition; //Mouse position in pixel coordinates. (0,0) is in top-left. 
	static Vec2 MouseWheelDelta;

	//NOTE: This really doesn't belong here. Should it go in renderer?
	static Vec2 ScreenSize; //Screen size, in pixels

	//TODO: Keyboard. Fun. Kind of want A "GetButton" function, but for now, 
	//just do an array with the keyboard status.
	//Two things are useful: Was the button pressed/released this frame, 
	//and what is its current state? So need to store two arrays for each button.

	//These only include physical keys that don't need a modifier (shift, alt, etc)
	enum class Keyname
	{
		//Basic ascii keys
		SPACE = 32,
		APOSTROPHE = 39,
		COMMA = 44, MINUS, PERIOD, SLASH,
		ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE,
		SEMICOLON = 59, EQUAL = 61,
		A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		LEFT_BRACKET, BACKSLASH, RIGHT_BRACKET,
		GRAVE_ACCENT = 96,

		//Function keys.
		KEY_ESCAPE = 256, ENTER, TAB, BACKSPACE, INSERT, DELETE_KEY,
		RIGHT, LEFT, DOWN, UP, PAGE_UP, PAGE_DOWN, HOME, END,
		CAPS_LOCK = 280, SCROLL_LOCK, NUM_LOCK, PRINT_SCREEN, PAUSE,
		F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		KEYPAD0 = 320, KEYPAD1, KEYPAD2, KEYPAD3, KEYPAD4, KEYPAD5, KEYPAD6, KEYPAD7, KEYPAD8, KEYPAD9,
		KEYPAD_DECIMAL, KEYPAD_DIVIDE, KEYPAD_MULTIPLY, KEYPAD_SUBTRACT, KEYPAD_ADD, KEYPAD_ENTER, KEYPAD_EQUAL, 
		LEFT_SHIFT = 340, LEFT_CONTROL, LEFT_ALT, LEFT_SUPER, RIGHT_SHIFT, RIGHT_CONTROL, RIGHT_ALT, RIGHT_SUPER, MENU,
		NUM_KEYNAMES //Not strictly true, but oh well.
	};

	enum class KeyAction
	{
		RELEASED, PRESSED, REPEAT
	};

	static void NewFrame();
	static void SetKeyStatus(Keyname key, KeyAction action);

	static bool GetKey(Keyname key);
	static bool GetKeyDown(Keyname key);
	static bool GetKeyUp(Keyname key);

	using ResizeCallback = void(*)(int w, int h, void* data);
	static void RegisterResizeCallback(ResizeCallback callback, void* user_data = nullptr);
	static void ResizeScreen(int w, int h);

};

}