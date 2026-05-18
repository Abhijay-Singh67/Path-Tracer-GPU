#version 460 core

out vec4 FragColor;

in vec2 DisplayTexCoords;

uniform sampler2D displayTexture;
uniform float exposure;

//ACES Filmic tonemapper

vec3 acesToneMapper(vec3 color){
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(displayTexture, DisplayTexCoords).rgb;

    //Apply exposure
    hdr *= exposure;

    //Tonemap from HDR to LDR
    vec3 ldr = acesToneMapper(hdr);
    
    //Gamma Correction
    ldr = pow(ldr, vec3(1.0 / 2.2));
    
    FragColor = vec4(ldr, 1.0);
}