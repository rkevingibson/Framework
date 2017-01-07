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

		inline float Length() const noexcept { return std::sqrt(x*x + y*y); }

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
		inline friend float Dot(const Vec2& lhs, const Vec2& rhs) noexcept {
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
		
		inline float Length() const noexcept { return std::sqrt(x*x + y*y + z*z); }
	
		inline friend bool operator==(const Vec3& lhs, const Vec3& rhs) {
			return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
		}

		inline friend bool operator!=(const Vec3& lhs, const Vec3& rhs) {
			return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
		}

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
		inline friend float Dot(const Vec3& lhs, const Vec3& rhs) noexcept {
			return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z;
		}
		//Vector cross product
		inline friend Vec3 Cross(const Vec3& u, const Vec3& v) {
			Vec3 result;
			result.x = u.y*v.z - u.z*v.y;
			result.y = u.z*v.x - u.x*v.z;
			result.z = u.x*v.y - u.y*v.x;
			return result;
		}
	
		inline friend Vec3 Normalize(const Vec3& a) {
			auto l = a.Length();
			return Vec3(a.x / l, a.y / l, a.z / l);
		}
	
		inline friend Vec3 Min(const Vec3& a, const Vec3& b) {
			return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
		}
	
		inline friend Vec3 Max(const Vec3& a, const Vec3& b) {
			return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
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
		friend float Dot(const Vec4& lhs, const Vec4& rhs) noexcept {
			return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z + lhs.w*rhs.w;
		}
	};

	struct Mat3
	{
		float data[9]{};

		//=====================Access operators=====================
		inline float& operator()(unsigned int row, unsigned int col)
		{
			return data[3 * col + row];
		}

		inline const float& operator()(unsigned int row, unsigned int col) const
		{
			return data[3 * col + row];
		}

		inline float& operator[](unsigned int idx) 
		{
			return data[idx];
		}

		inline const float& operator[](unsigned int idx) const
		{
			return data[idx];
		}

		inline friend Mat3 operator*(const Mat3& lhs, const Mat3& rhs)
		{
			Mat3 x;
			x[0] = lhs[0] * rhs[0] + lhs[3] * rhs[1] + lhs[6] * rhs[2];
			x[1] = lhs[1] * rhs[0] + lhs[4] * rhs[1] + lhs[7] * rhs[2];
			x[2] = lhs[2] * rhs[0] + lhs[5] * rhs[1] + lhs[8] * rhs[2]; 
			x[3] = lhs[0] * rhs[3] + lhs[3] * rhs[4] + lhs[6] * rhs[5];
			x[4] = lhs[1] * rhs[3] + lhs[4] * rhs[4] + lhs[7] * rhs[5];
			x[5] = lhs[2] * rhs[3] + lhs[5] * rhs[4] + lhs[8] * rhs[5];
			x[6] = lhs[0] * rhs[6] + lhs[3] * rhs[7] + lhs[6] * rhs[8];
			x[7] = lhs[1] * rhs[6] + lhs[4] * rhs[7] + lhs[7] * rhs[8];
			x[8] = lhs[2] * rhs[6] + lhs[5] * rhs[7] + lhs[8] * rhs[8];
			return x;
		}

		inline friend Mat3 operator*(const Mat3& lhs, float rhs) noexcept
		{
			Mat3 x;
			for (int i = 0; i < 9; i++)
			{
				x[i] = rhs*lhs[i];
			}
			return x;
		}

		inline friend Mat3 operator*(float lhs, const Mat3& rhs) noexcept {
			return rhs*lhs;
		}

		inline float Determinant() const noexcept
		{
			return data[0] * data[4] * data[8] + data[3] * data[7] * data[2] + data[6] * data[1] * data[5]
				 - data[0] * data[7] * data[5] - data[3] * data[1] * data[8] - data[6] * data[4] * data[2];
		}

		//Returns the inverse, or the zero matrix if non-invertible.
		inline friend Mat3 InverseOrZero(const Mat3& x) noexcept
		{
			float det = x.Determinant();
			Mat3 result;
			if (det < std::numeric_limits<float>::epsilon()) {
				memset(result.data, 0, sizeof(result.data));
				return result;
			}
			result[0] = x[4] * x[8] - x[7] * x[5];
			result[1] = x[7] * x[2] - x[1] * x[8];
			result[2] = x[1] * x[5] - x[4] * x[2];
			result[3] = x[6] * x[5] - x[3] * x[8];
			result[4] = x[0] * x[8] - x[6] * x[2];
			result[5] = x[3] * x[2] - x[0] * x[5];
			result[6] = x[3] * x[7] - x[6] * x[4];
			result[7] = x[6] * x[1] - x[0] * x[7];
			result[8] = x[0] * x[4] - x[3] * x[1];
			return (1.f / det)*result;
		}


	};

	struct Mat4 
	{
		float data[16];

		inline void SetZero()
		{
			memset(data, 0, sizeof(data));
		}

		inline float& operator()(unsigned int row, unsigned int col)
		{
			return data[4 * col + row];
		}

		inline const float& operator()(unsigned int row, unsigned int col) const
		{
			return data[4 * col + row];
		}

		inline float& operator[](unsigned int idx)
		{
			return data[idx];
		}

		inline const float& operator[](unsigned int idx) const
		{
			return data[idx];
		}


		inline friend Mat4 operator*(const Mat4& lhs, const Mat4& rhs)
		{
			Mat4 x;
			//Sure, this makes more sense than using a loop...
			x[0] = lhs[0] * rhs[0] + lhs[4] * rhs[1] + lhs[8] * rhs[2] + lhs[12] * rhs[3];
			x[1] = lhs[1] * rhs[0] + lhs[5] * rhs[1] + lhs[9] * rhs[2] + lhs[13] * rhs[3];
			x[2] = lhs[2] * rhs[0] + lhs[6] * rhs[1] + lhs[10] * rhs[2] + lhs[14] * rhs[3];
			x[3] = lhs[3] * rhs[0] + lhs[7] * rhs[1] + lhs[11] * rhs[2] + lhs[15] * rhs[3];
			
			x[4] = lhs[0] * rhs[4] + lhs[4] * rhs[5] + lhs[8] * rhs[6] + lhs[12] * rhs[7];
			x[5] = lhs[1] * rhs[4] + lhs[5] * rhs[5] + lhs[9] * rhs[6] + lhs[13] * rhs[7];
			x[6] = lhs[2] * rhs[4] + lhs[6] * rhs[5] + lhs[10] * rhs[6] + lhs[14] * rhs[7];
			x[7] = lhs[3] * rhs[4] + lhs[7] * rhs[5] + lhs[11] * rhs[6] + lhs[15] * rhs[7];

			x[8]  = lhs[0] * rhs[8] + lhs[4] * rhs[9] + lhs[8] * rhs[10] + lhs[12] * rhs[11];
			x[9]  = lhs[1] * rhs[8] + lhs[5] * rhs[9] + lhs[9] * rhs[10] + lhs[13] * rhs[11];
			x[10] = lhs[2] * rhs[8] + lhs[6] * rhs[9] + lhs[10] * rhs[10] + lhs[14] * rhs[11];
			x[11] = lhs[3] * rhs[8] + lhs[7] * rhs[9] + lhs[11] * rhs[10] + lhs[15] * rhs[11];

			x[12] = lhs[0] * rhs[12] + lhs[4] * rhs[13] + lhs[8] * rhs[14] + lhs[12] * rhs[15];
			x[13] = lhs[1] * rhs[12] + lhs[5] * rhs[13] + lhs[9] * rhs[14] + lhs[13] * rhs[15];
			x[14] = lhs[2] * rhs[12] + lhs[6] * rhs[13] + lhs[10] * rhs[14] + lhs[14] * rhs[15];
			x[15] = lhs[3] * rhs[12] + lhs[7] * rhs[13] + lhs[11] * rhs[14] + lhs[15] * rhs[15];
			return x;
		}

	};

}