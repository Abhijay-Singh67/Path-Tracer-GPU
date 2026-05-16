#version 420 core

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
//Set the random state once for the PRNG
uint rngState = uint(gl_FragCoord.x) * 1973u + uint(gl_FragCoord.y) * 1920u + frameCount * 26699u;
//Constants
const float INF = 1.0 / 0.0 ;

//Includes
#include "common.glsl"
#include "headers.glsl"
#include "sphere.glsl"
#include "camera.glsl"
#include "interval.glsl"

void main() {
    //We first construct a ray
    ray r = generateCameraRay(TexCoord, gl_FragCoord);
    vec3 result = ray_color(r);

    //Frame accumulation
    vec3 prev = texture(prevFrameTexture, TexCoord).rgb;
    vec3 accum = (prev * float(frameCount - 1u) + result) / float(frameCount);
    FragColor = vec4(accum, 1.0f);
}