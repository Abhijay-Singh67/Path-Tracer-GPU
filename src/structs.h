#ifndef STRUCTS_H
#define STRUCTS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Material {
    glm::vec4 albedo; // xyz = color, w = material type
    glm::vec4 extra; //x = fuzz, y = refraction index, z = intensity of emission
    glm::vec4 absorption; //xyz = absorption coefficients per unit distance used for tinted glass};
};
static_assert(sizeof(Material) == 48, "");

struct GPUSphere {
    glm::vec4 center; //xyz = position, w = radius
    glm::vec4 extra; // x = material index
};
static_assert(sizeof(GPUSphere) == 32, "");

struct GPUQuad {
    glm::vec4 Q; // xyz = Position, w = material index
    glm::vec4 u; // xyz = direction
    glm::vec4 v; //xyz = direction
};
static_assert(sizeof(GPUQuad) == 48, "");

struct GPUVertex {
    glm::vec4 position; //xyz = position
    glm::vec4 normal; //xyz = normal
};
static_assert(sizeof(GPUVertex) == 32,"");

struct GPUIndex {
    glm::ivec4 index; //xyz = index, w = mat_index
};
static_assert(sizeof(GPUIndex) == 16, "");

struct GPUMediumSphere {
    glm::vec4 center;        // xyz = position, w = radius
    glm::vec4 albedo_density; // xyz = scattering albedo, w = density
};
static_assert(sizeof(GPUMediumSphere) == 32, "");

struct GPUBVHNode {
    glm::vec4 bbox_min;   // xyz = min, w = padding
    glm::vec4 bbox_max;   // xyz = max, w = padding
    glm::ivec4 data;      // x = left_child (interior) or first_ref (leaf)
                          // y = right_child (interior) or ref_count (leaf)
                          // z = is_leaf flag (0 = interior, 1 = leaf)
};
static_assert(sizeof(GPUBVHNode) == 48, "");

struct GPUPrimitiveRef {
    glm::ivec4 data;   // x = primitive_type, y = index, z/w = padding
};
static_assert(sizeof(GPUPrimitiveRef) == 16, "");

#endif 