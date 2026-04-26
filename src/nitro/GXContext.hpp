#pragma once
#include <vector>
#include <glm/glm.hpp>

struct GXContext
{
    glm::mat4 currentMatrix = glm::mat4(1.0f);
    std::vector<glm::mat4> matrixStack;

    glm::vec3 currentPos = {0, 0, 0};

    bool applyTransforms = true;
};