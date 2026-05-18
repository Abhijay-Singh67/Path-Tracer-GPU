#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#include <iostream>
#include <vector>
#include "stb_image.h"
#include "stb_image_resize2.h"

class TextureArray {
public:
    TextureArray(int width, int height, int max_layers)
        : width_(width), height_(height), max_layers_(max_layers), next_layer_(0)
    {
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, id_);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height, max_layers);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Returns the layer index of the loaded texture, or -1 on failure.
    // Automatically resizes images that don't match the array's dimensions.
    int load(const char* path) {
        if (next_layer_ >= max_layers_) {
            std::cerr << "Texture array full!" << std::endl;
            return -1;
        }

        int w, h, c;
        unsigned char* data = stbi_load(path, &w, &h, &c, 4);
        if (!data) {
            std::cerr << "Failed to load " << path << std::endl;
            return -1;
        }

        unsigned char* upload_data = data;
        std::vector<unsigned char> resized_buffer;  // only allocated if resize needed

        if (w != width_ || h != height_) {
            std::cout << "Resizing " << path << " from " << w << "x" << h
                      << " to " << width_ << "x" << height_ << std::endl;
            
            resized_buffer.resize(width_ * height_ * 4);
            
            // stbir_resize_uint8_srgb handles sRGB-aware resampling for color textures.
            // For non-color data (normal maps, roughness maps), use stbir_resize_uint8_linear.
            unsigned char* result = stbir_resize_uint8_srgb(
                data, w, h, 0,                    // source: pixels, w, h, stride (0 = tightly packed)
                resized_buffer.data(), width_, height_, 0,  // dest: pixels, w, h, stride
                STBIR_RGBA                         // pixel layout
            );
            
            if (!result) {
                std::cerr << "Failed to resize " << path << std::endl;
                stbi_image_free(data);
                return -1;
            }
            
            upload_data = resized_buffer.data();
        }

        glBindTexture(GL_TEXTURE_2D_ARRAY, id_);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                        0, 0, next_layer_,
                        width_, height_, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, upload_data);

        stbi_image_free(data);
        // resized_buffer cleans up automatically when it goes out of scope
        
        return ++next_layer_;
    }

    unsigned int id() const { return id_; }

private:
    unsigned int id_;
    int width_, height_, max_layers_, next_layer_;
};

#endif