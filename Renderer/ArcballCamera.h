#pragma once

#include "Utilities/Geometry.h"

namespace rkg {
/*
    Simple camera with arcball controls, forming a spherical coordinate system around a target.
*/
Mat4 ConstructView(const rkg::Vec3& origin, const rkg::Vec3& target, const rkg::Vec3& up);

class ArcballCamera {
   public:
    Mat4 GetViewMatrix() const;

    // Takes mouse pos in [-1,1] range.
    void        StartArcball(const Vec2& mouse_pos);
    void        UpdateArcball(const Vec2& mouse_pos);
    inline void EndArcball() { arcball_active = false; };

    Vec3  target{0, 0, 0};
    float distance{1};
    float theta{0};  // Angle from horizon
    float phi{0};    // Angle from front

    Vec2                   screen_size{1080, 920};
    static constexpr float move_speed{3.0f};
    static constexpr float scroll_speed{0.05f};

   private:
    bool arcball_active{false};
    Vec2 last_mouse_pos;
};
}  // namespace rkg
