#version 460 core
//Essential global includes are up here
#include "common.glsl"
#include "headers.glsl"

in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D prevFrameTexture;

layout (std140, binding = 0) uniform Camera{
    vec4 camPosition;
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 cameraForward;
    int WIDTH;
    int HEIGHT;
    float fov;
    uint frameCount;
    float defocus_angle; //in radians
    float focus_dist;
    vec2 _pad;
};

layout (std430, binding = 0) readonly buffer SphereBuffer {
    GPUSphere spheres[];
};

layout (std430, binding = 1) readonly buffer MaterialBuffer {
    Material materials[];
};

layout (std430, binding = 2) readonly buffer QuadBuffer {
    GPUQuad quads[];
};

layout (std430, binding = 3) readonly buffer VertexBuffer {
    GPUVertex vertices[];
};

layout (std430, binding = 4) readonly buffer IndexBuffer {
    GPUIndex indices[];
};

layout (std430, binding = 5) readonly buffer BVHBuffer {
    BVHNode bvhNodes[];
};

layout (std430, binding = 6) readonly buffer RefsBuffer {
    PrimRef primRefs[];
};

//Includes
#include "hittables.glsl"
#include "interval.glsl"
#include "AABB.glsl"
#include "materials.glsl"
#include "camera.glsl"

void main() {
    //Set the random state once for the PRNG
    uint seed = uint(gl_FragCoord.x) * 2654435761u 
              + uint(gl_FragCoord.y) * 2246822519u 
              + frameCount * 3266489917u;
    rngState = pcg_hash(pcg_hash(seed));
    rand(rngState); 

    //We first construct a ray
    ray r = generateCameraRay(gl_FragCoord);
    vec3 result = ray_color(r);

    //Frame accumulation
    vec3 prev = texture(prevFrameTexture, TexCoord).rgb;
    vec3 accum = (prev * float(frameCount - 1u) + result) / float(frameCount);
    FragColor = vec4(accum, 1.0);
}