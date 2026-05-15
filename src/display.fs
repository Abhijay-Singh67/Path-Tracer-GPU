#version 420 core

out vec4 FragColor;

in vec2 DisplayTexCoords;

uniform sampler2D displayTexture;

void main() {
    FragColor = texture(displayTexture, DisplayTexCoords);
}