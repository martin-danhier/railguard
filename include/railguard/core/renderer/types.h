#pragma once

#include <cstdint>

namespace rg
{
    using RenderStageId = uint16_t;

    // ---=== Structs ===---

    struct Version
    {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t patch = 0;
    };

    // --=== Enums ===--

    enum class RenderStageKind
    {
        INVALID = 0,
        /** Geometry stage in deferred rendering. */
        DEFERRED_GEOMETRY,
        /** Lighting stage in deferred rendering. */
        DEFERRED_LIGHTING,
    };

    enum class Format
    {
        UNDEFINED = 0,
        /** Special format that will be replaced by the actual window format inferred by the renderer. */
        WINDOW_FORMAT,
        D32_SFLOAT,
        B8G8R8A8_SRGB,
        R8G8B8A8_SRGB,
        R8G8B8A8_UINT,
        R16G16B16A16_SFLOAT,
    };

    /** Describes how the the data should be arranged. */
    enum class ImageLayout
    {
        UNDEFINED                = 0,
        SHADER_READ_ONLY_OPTIMAL = 1,
        PRESENT_SRC              = 2,
        DEPTH_STENCIL_OPTIMAL    = 3,
    };

    enum class FilterMode
    {
        NEAREST = 0,
        LINEAR  = 1,
    };

    enum class ShaderStage : uint32_t
    {
        INVALID  = 0,
        VERTEX   = 1,
        FRAGMENT = 2,
    };
    // Operators to make it usable as flags to define several stages at once
    constexpr ShaderStage operator|(ShaderStage a, ShaderStage b)
    {
        return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    constexpr bool operator&(ShaderStage a, ShaderStage b)
    {
        return static_cast<bool>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
} // namespace rg