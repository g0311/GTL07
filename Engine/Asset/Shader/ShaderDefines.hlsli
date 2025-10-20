// ShaderDefines.hlsli
// : Global feature flags and constants for all shaders.

#ifndef SHADER_DEFINES_HLSLI
#define SHADER_DEFINES_HLSLI

// ============================================================================
// LIGHTING MODELS
// ============================================================================
// Default Lighting Model Selection
// Validation: only one lighting model should be active

#if !defined(UNLIT) && !defined(LIGHTING_MODEL_GOURAUD) && \
    !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && \
    !defined(LIGHTING_MODEL_BLINNPHONG)
#define LIGHTING_MODEL_PHONG 1
#endif

#if (defined(UNLIT) + defined(LIGHTING_MODEL_GOURAUD) + defined(LIGHTING_MODEL_LAMBERT) + \
     defined(LIGHTING_MODEL_PHONG) + defined(LIGHTING_MODEL_BLINNPHONG)) != 1
    #error "Exactly one of LIGHTING_MODEL_* must be defined."
#endif

// ============================================================================
// LIGHTING LIMITS
// ============================================================================
// Global limits for number of active lights.

#define NUM_POINT_LIGHT   8
#define NUM_SPOT_LIGHT    8
#define NUM_AMBIENT_LIGHT 8

// ============================================================================
// MATERIAL FLAGS (bitmask for MaterialFlags)
// ============================================================================
// Used in MaterialConstants.cbuffer to control which textures are active.

#define HAS_DIFFUSE_MAP   (1 << 0)
#define HAS_AMBIENT_MAP   (1 << 1)
#define HAS_SPECULAR_MAP  (1 << 2)
#define HAS_NORMAL_MAP    (1 << 3)
#define HAS_ALPHA_MAP     (1 << 4)
#define HAS_BUMP_MAP      (1 << 5)
#define UNLIT             (1 << 6)

// ============================================================================
// LIGHT TYPES
// ============================================================================
// Corresponds to ELightType enum in C++

#define LIGHT_TYPE_AMBIENT     0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT       2
#define LIGHT_TYPE_SPOT        3

// ============================================================================
// TILED LIGHTING / LIGHT CULLING
// ============================================================================
// Defines the screen tile size for the light culling algorithm.

#define TILE_SIZE 32

#endif // SHADER_DEFINES_HLSLI