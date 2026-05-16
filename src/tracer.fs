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
};

layout (std430, binding = 0) readonly buffer SphereBuffer {
    GPUSphere spheres[];
};

//Includes
#include "sphere.glsl"
#include "camera.glsl"
#include "interval.glsl"

void main() {
    //Set the random state once for the PRNG
    rngState = uint(gl_FragCoord.x) * 1973u + uint(gl_FragCoord.y) * 9277u + frameCount * 26699u;
    //We first construct a ray
    ray r = generateCameraRay(TexCoord, gl_FragCoord);
    vec3 result = ray_color(r);

    //Frame accumulation
    vec3 prev = texture(prevFrameTexture, TexCoord).rgb;
    vec3 accum = (prev * float(frameCount - 1u) + result) / float(frameCount);
    FragColor = vec4(accum, 1.0f);
}