#version 460 core

out vec4 FragColor;

in vec2 DisplayTexCoords;

uniform sampler2D displayTexture;

void main() {
    vec3 color = texture(displayTexture, DisplayTexCoords).rgb;
    
    //Gamma Correction
    color = pow(color, vec3(1.0 / 2.0));
    
    FragColor = vec4(color, 1.0);
}