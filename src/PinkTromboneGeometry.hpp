#pragma once
#include <rack.hpp>

// Coordinate system ported from Neil Thapen's original 600 x 600 canvas.
// See LICENSE-PinkTrombone-UI.txt and the source attribution in PinkTromboneUI.hpp.
namespace pinkGeometry {
constexpr float originX = 340.f, originY = 449.f;
constexpr float radius = 298.f, scale = 60.f;
constexpr float angleOffset = -.24f, angleScale = .64f;
inline rack::math::Vec point(float index, float diameter) {
    float angle = angleOffset + index * angleScale * float(M_PI) / 38.f;
    float r = radius - scale * diameter;
    return rack::math::Vec(originX - r * std::cos(angle), originY - r * std::sin(angle));
}
inline float index(rack::math::Vec point) {
    float angle = std::atan2(point.y - originY, point.x - originX);
    if (angle > 0.f) angle -= 2.f * float(M_PI);
    return (float(M_PI) + angle - angleOffset) * 38.f / (angleScale * float(M_PI));
}
inline float diameter(rack::math::Vec point) {
    float x = point.x - originX, y = point.y - originY;
    return (radius - std::sqrt(x * x + y * y)) / scale;
}
}
