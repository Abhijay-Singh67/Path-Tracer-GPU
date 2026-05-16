#version 420 core

in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D prevFrameTexture; //not used as of now

layout (std140, binding = 0) uniform Camera{
    vec4 camPosition;
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 cameraForward;
    int WIDTH;
    int HEIGHT;
    float fov;
    float _pad;
};

//Includes
#include "common.glsl"
#include "sphere.glsl"
#include "camera.glsl"

void main() {
    //We first construct a ray
    ray r = generateCameraRay(TexCoord);
    FragColor = vec4(ray_color(r),1.0f);
}