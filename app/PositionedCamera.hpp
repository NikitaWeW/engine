#pragma once
#include "glm/gtc/quaternion.hpp"
#include "engine/dsa/ECS.hpp"

struct PositionedCamera {
    glm::vec3 position;
    glm::quat orientation;

    float fov = 90.0f;
    float ratio = 1.0f;
    float znear = 0.01f;
    float zfar = 1000.0f;

    static void update(egn::Registry &reg);
};
