#include "Geometry.h"

const rkg::Mat3 rkg::Mat3::Identity{ Vec3(1.0,0.0,0.0), 
									 Vec3(0.0,1.0,0.0), 
									 Vec3(0.0,0.0,1.0) };

const rkg::Mat4 rkg::Mat4::Identity{ Vec4(1.0,0.0,0.0,0.0),
									 Vec4(0.0,1.0,0.0,0.0),
									 Vec4(0.0,0.0,1.0,0.0),
									 Vec4(0.0,0.0,0.0,1.0) };

