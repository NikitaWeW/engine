#pragma once
#include "engine/io/Window.hpp"
#include "GLFW/glfw3.h"
#include "PositionedCamera.hpp"

class Controller {
public:
    struct Camera
    {
        egn::Entity window;

        float speed = 5;
        float boost = 20;
        float sensitivity = 0.1;
        bool firstTimeMovingMouse = true;
        bool locked = true;
    };

    static egn::Entity createCamera(egn::RestrictedEntity<egn::Window> window, glm::vec3 pos = {0, 0, 0}, glm::vec3 target = {0, 0, -10});

    void update(egn::Registry &reg, float dt);
};
