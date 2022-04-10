#pragma once

#include <railguard/utils/geometry/mat4.h>

namespace rg {
    struct GPUCameraData
    {
        Mat4 view            = {};
        Mat4 projection      = {};
        Mat4 view_projection = {};
    };
}