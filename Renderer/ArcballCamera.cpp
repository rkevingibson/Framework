#include "ArcballCamera.h"

#define M_PI	3.14159265358979323846 

namespace rkg 
{

namespace
{


rkg::Vec3 GetArcballVector(const rkg::Vec2& pos, const rkg::Vec2& screen_size)
{
	using namespace rkg;
	Vec3 P(1.0f*pos.x / screen_size.x * 2 - 1.0f, -1.0f*pos.y / screen_size.y * 2 + 1.0f, 0);
	float OP_squared = P.x*P.x + P.y*P.y;
	if (OP_squared <= 1.0) {
		P.z = sqrtf(1.0f - OP_squared);
	}
	return Normalize(P);
}
}

Mat4 ConstructView(const rkg::Vec3& origin, const rkg::Vec3& target, const rkg::Vec3& up)
{
	using namespace rkg;
	Mat4 view;
	const Vec3 zaxis = Normalize(origin - target); // -Z is away from camera.
	const Vec3 xaxis = Normalize(Cross(up, zaxis));
	const Vec3 yaxis = Normalize(Cross(zaxis, xaxis));

	view(0, 0) = xaxis.x;	view(0, 1) = xaxis.y;	view(0, 2) = xaxis.z;	view(0, 3) = -Dot(xaxis, origin);
	view(1, 0) = yaxis.x;	view(1, 1) = yaxis.y;	view(1, 2) = yaxis.z;	view(1, 3) = -Dot(yaxis, origin);
	view(2, 0) = zaxis.x;	view(2, 1) = zaxis.y;	view(2, 2) = zaxis.z;	view(2, 3) = -Dot(zaxis, origin);
	view(3, 0) = 0;			view(3, 1) = 0;			view(3, 2) = 0;			view(3, 3) = 1.f;
	return view;
}

Mat4 ArcballCamera::GetViewMatrix() const
{
	//Construct the view matrix from our parameters.
	//We have polar coordinates, so we can compute our position, relative to the target.
	//Then we can orient ourselves, since we want to be looking at the target. So that's easy enough.
	//First, construct the rotation around the origin, then add the translations together. Super simple.
	//Assume we start with the camera looking straight down (-Z direction), with the "Up" of the camera pointing in the +X direction.
	//First, we rotate around the Z axis by phi_
	//We then want to rotate first around the +Y axis by theta_.
	//I think... Not super confident about this.
	Mat4 view;
	float cphi = cos(phi);
	float sphi = sin(phi);
	float ctheta = cos(theta);
	float stheta = sin(theta);
	Vec3 sphere_pos(distance*ctheta*sphi, distance*stheta, distance*ctheta*cphi);
	Vec3 up(0, ctheta, 0);
	view = ConstructView(target + sphere_pos, target, up);
	return view;
}

void ArcballCamera::StartArcball(const Vec2& mouse_pos)
{
	arcball_active = true;
	last_mouse_pos = mouse_pos;
}

void ArcballCamera::UpdateArcball(const Vec2& mouse_pos)
{
	if (!arcball_active) { return; }

	const rkg::Vec3 va = GetArcballVector(last_mouse_pos, screen_size);
	const rkg::Vec3 vb = GetArcballVector(mouse_pos, screen_size);
	auto diff = va - vb;

	//Decompose diff into horizontal/vertical movement.
	auto delta_theta = diff.y;
	auto delta_phi = diff.x;
	last_mouse_pos = mouse_pos;

	theta = std::fmod(theta + move_speed*delta_theta, (float)2*M_PI);
	phi = std::fmod(phi + move_speed*delta_phi, (float)2*M_PI);

}

}