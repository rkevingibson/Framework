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

//TODO: Keyboard. Fun. Kind of want A "GetButton" function, but for now, just do an array with the keyboard status.
//Two things are useful: Was the button pressed/released this frame, and what is its current state? So need to store two arrays for each button.


};

}