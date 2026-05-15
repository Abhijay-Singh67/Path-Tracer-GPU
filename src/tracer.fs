#version 420 core

in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D prevFrameTexture; //not used as of now

void main() {
    FragColor = vec4(TexCoord, 0.0, 1.0);
}