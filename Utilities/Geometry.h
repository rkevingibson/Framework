#pragma once
#include <algorithm>

namespace rkg {
	//Includes some basic geometric primitives - vectors, matrices. All fixed size here.

	struct Vec2 {
		float x, y;
		inline Vec2() = default;
		inline Vec2(const Vec2&) = default;
		inline Vec2(Vec2&&) = default;
		inline Vec2(float x_, float y_) : x{ x_ }, y{ y_ }{}
		inline Vec2(const float data[2]) : x{ data[0] }, y{ data[1] } {}

		Vec2& operator=(const Vec2& rhs) = default;
		Vec2& operator=(Vec2&& rhs) = default;

		inline float length() const noexcept { return std::sqrt(x*x + y*y); }

		//====================Comparison operators==================
		inline friend bool operator==(const Vec2& lhs, const Vec2& rhs) {
			return lhs.x == rhs.x && lhs.y == rhs.y;
		}

		inline friend bool operator!=(const Vec2& lhs, const Vec2& rhs) {
			return lhs.x != rhs.x || lhs.y != rhs.y;
		}

		//====================Compound Operators====================
		//Vector addition
		inline Vec2& operator+=(const Vec2& rhs) {
			x += rhs.x; y += rhs.y;
			return *this;
		}
		//Vector subtraction
		inline Vec2& operator-=(const Vec2& rhs) {
			x -= rhs.x; y -= rhs.y;
			return *this;
		}
		//Scalar multiplication
		inline Vec2& operator*=(float rhs) {
			x *= rhs; y *= rhs;
			return *this;
		}
		//Scalar division
		inline Vec2& operator/=(float rhs) {
			x /= rhs; y /= rhs;
			return *this;
		}

		//==================== Binary Operators ====================
		inline friend Vec2 operator+(Vec2 lhs, const Vec2& rhs) noexcept {
			lhs += rhs;
			return lhs;
		}
		//Vector subtraction
		inline friend Vec2 operator-(Vec2 lhs, const Vec2& rhs) noexcept {
			lhs -= rhs;
			return lhs;
		}
		//Scalar multiplication
		inline friend Vec2 operator*(Vec2 lhs, float rhs) noexcept {
			lhs *= rhs;
			return lhs;
		}
		//Scalar multiplication
		inline friend Vec2 operator*(float lhs, Vec2 rhs) noexcept {
			rhs *= lhs;
			return rhs;
		}
		//Scalar division
		inline friend Vec2 operator/(Vec2 lhs, float rhs) {
			lhs /= rhs;
			return lhs;
		}
		//Vector dot product
		inline friend float dot(const Vec2& lhs, const Vec2& rhs) noexcept {
			return lhs.x*rhs.x + lhs.y*rhs.y;
		}

	};

	struct Vec3 {
		float x, y, z;
		inline Vec3() = default;
		inline Vec3(const Vec3&) = default;
		inline Vec3(Vec3&&) = default;
		inline Vec3(float x_, float y_, float z_) : x { x_ }, y{ y_ }, z{ z_ } {}
		inline Vec3(const float* data) : x{ data[0] }, y{ data[1] }, z{ data[2] } {}
	
		Vec3& operator=(const Vec3& rhs) = default;
		Vec3& operator=(Vec3&& rhs) = default;
		
		inline float length() const noexcept { return std::sqrt(x*x + y*y + z*z); }
	
		//====================Compound Operators====================//

		//Vector addition
		inline Vec3& operator+=(const Vec3& rhs) noexcept {
			x += rhs.x; y += rhs.y; z += rhs.z;
			return *this;
		}
		//Vector subtraction
		inline Vec3& operator-=(const Vec3& rhs) noexcept {
			x -= rhs.x; y -= rhs.y; z -= rhs.z;
			return *this;
		}
		//Scalar multiplication
		inline Vec3& operator*=(float rhs) noexcept{
			x *= rhs; y *= rhs; z *= rhs;
			return *this;
		}
		//Scalar division
		inline Vec3& operator/=(float rhs) noexcept {
			x /= rhs; y /= rhs; z /= rhs;
			return *this;
		}

		//====================Binary Operators====================//
		//Implement non-members as friends, in terms of the compound operators. Takes advantage of parameter copying.

		//Vector addition
		inline friend Vec3 operator+(Vec3 lhs, const Vec3& rhs) noexcept {
			lhs += rhs;
			return lhs;
		}
		//Vector subtraction
		inline friend Vec3 operator-(Vec3 lhs, const Vec3& rhs) noexcept {
			lhs -= rhs;
			return lhs;
		}
		//Scalar multiplication
		inline friend Vec3 operator*(Vec3 lhs, float rhs) noexcept {
			lhs *= rhs;
			return lhs;
		}
		//Scalar multiplication
		inline friend Vec3 operator*(float lhs, Vec3 rhs) noexcept {
			rhs *= lhs;
			return rhs;
		}
		//Scalar division
		inline friend Vec3 operator/(Vec3 lhs, float rhs) {
			lhs /= rhs;
			return lhs;
		}
		//Vector dot product
		inline friend float dot(const Vec3& lhs, const Vec3& rhs) noexcept {
			return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z;
		}
		//Vector cross product
		inline friend Vec3 cross(const Vec3& u, const Vec3& v) {
			Vec3 result;
			result.x = u.y*v.z - u.z*v.y;
			result.y = u.z*v.x - u.x*v.z;
			result.z = u.x*v.y - u.y*v.x;
			return result;
		}
	
		inline friend Vec3 normalize(const Vec3& a) {
			auto l = a.length();
			return Vec3(a.x / l, a.y / l, a.z / l);
		}
	
		inline friend Vec3 min(const Vec3& a, const Vec3& b) {
			return Vec3(std::min({ a.x, b.x }), std::min({ a.y, b.y }), std::min({ a.z, b.z }));
		}
	
		inline friend Vec3 max(const Vec3& a, const Vec3& b) {
			return Vec3(std::max({ a.x,b.x }), std::max({ a.y,b.y }), std::max({ a.z,b.z }));
		}
	};

	struct Vec4 {
		float x, y, z, w;

		//====================Compound Operators====================//

		//Vector addition
		inline Vec4& operator+=(const Vec4& rhs) {
			x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w;
			return *this;
		}
		//Vector subtraction
		inline Vec4& operator-=(const Vec4& rhs) {
			x -= rhs.x; y -= rhs.y; z -= rhs.z; w += rhs.w;
			return *this;
		}
		//Scalar multiplication
		inline Vec4& operator*=(float rhs) {
			x *= rhs; y *= rhs; z *= rhs; w += rhs;
			return *this;
		}
		//Scalar division
		inline Vec4& operator/=(float rhs) {
			x /= rhs; y /= rhs; z /= rhs; w /= rhs;
			return *this;
		}

		//====================Binary Operators====================//
		//Implement non-members as friends, in terms of the compound operators. Takes advantage of parameter copying.

		//Vector addition
		friend Vec4 operator+(Vec4 lhs, const Vec4& rhs) noexcept {
			lhs += rhs;
			return lhs;
		}
		//Vector subtraction
		friend Vec4 operator-(Vec4 lhs, const Vec4& rhs) noexcept {
			lhs -= rhs;
			return lhs;
		}
		//Scalar multiplication
		friend Vec4 operator*(Vec4 lhs, float rhs) noexcept {
			lhs *= rhs;
			return lhs;
		}
		//Scalar multiplication
		friend Vec4 operator*(float lhs, Vec4 rhs) noexcept {
			rhs *= lhs;
			return rhs;
		}
		//Scalar division
		friend Vec4 operator/(Vec4 lhs, float rhs) {
			lhs /= rhs;
			return lhs;
		}
		//Vector dot product
		friend float dot(const Vec4& lhs, const Vec4& rhs) noexcept {
			return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z + lhs.w*rhs.w;
		}
	};

	struct Mat4 
	{
		float data[16];

		inline float& operator()(unsigned int row, unsigned int col)
		{
			return data[4 * col + row];
		}

		inline const float& operator()(unsigned int row, unsigned int col) const
		{
			return data[4 * col + row];
		}

	};

}