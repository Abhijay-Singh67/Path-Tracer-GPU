#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <set>
namespace fs = std::filesystem;

class Shader{
    public:
        unsigned int ID; //the program ID

        //constrcutor reads and builds the shader
        Shader(const char* vertexPath, const char* fragmentPath, bool debug) {
            // 1. retrieve and preprocess the vertex/fragment source code
            std::string vertexCode   = preprocess(vertexPath);
            std::string fragmentCode = preprocess(fragmentPath);
            if(debug){
                std::cout << "===== FRAGMENT SHADER =====\n";
                {
                    int line = 1;
                    std::istringstream iss(fragmentCode);
                    std::string l;
                    while (std::getline(iss, l)) {
                        std::cout << line++ << ": " << l << "\n";
                    }
                }
                std::cout << "===========================\n";
            }
            if (vertexCode.empty() || fragmentCode.empty()) {
                std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
                return;
            }

            const char* vShaderCode = vertexCode.c_str();
            const char* fShaderCode = fragmentCode.c_str();

            // 2. Compile Shaders
            unsigned int vertex, fragment;
            int success;
            char infolog[512];

            // vertex Shader
            vertex = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertex, 1, &vShaderCode, NULL);
            glCompileShader(vertex);
            glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(vertex, 512, NULL, infolog);
                std::cout << "ERROR::SHADER::VERTEX" << vertexPath << "::COMPILATION_FAILED\n" << infolog << std::endl;
            }

            // fragment Shader
            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragment, 1, &fShaderCode, NULL);
            glCompileShader(fragment);
            glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(fragment, 512, NULL, infolog);
                std::cout << "ERROR::SHADER::FRAGMENT::" << fragmentPath <<"::COMPILATION_FAILED\n" << infolog << std::endl;
            }

            // Shader Program
            ID = glCreateProgram();
            glAttachShader(ID, vertex);
            glAttachShader(ID, fragment);
            glLinkProgram(ID);
            glGetProgramiv(ID, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(ID, 512, NULL, infolog);
                std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infolog << std::endl;
            }

            glDeleteShader(vertex);
            glDeleteShader(fragment);
        }

        //use/activate the shader
        void use(){
            glUseProgram(ID);
        }

        //utility uniform functions
        void setBool(const std::string &name, bool value) const{
            glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
        } 
        void setInt(const std::string &name, int value) const {
            glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
        }   
        void setFloat(const std::string &name, float value) const {
            glUniform1f(glGetUniformLocation(ID, name.c_str()), (float)value);
        }
        void setVec3(const std::string &name,const glm::vec3& value) const {
            glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
        }
        void setVec3(const std::string &name, double x, double y, double z) const {
            glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
        }
        void setMat4(const std::string &name, const glm::mat4& value) const {
            glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
        }

    private:
        // Public entry — sets up the "seen" set for include-guard tracking
    static std::string preprocess(const std::string& path) {
        std::set<std::string> seen;
        return preprocessImpl(path, seen);
    }

    static std::string preprocessImpl(const std::string& path, std::set<std::string>& seen) {
        // include-guard: skip if already pulled in
        std::string absPath;
        try {
            absPath = fs::absolute(path).string();
        } catch (...) {
            std::cout << "ERROR::SHADER::INVALID_PATH: " << path << std::endl;
            return "";
        }
        if (seen.count(absPath)) return "";
        seen.insert(absPath);

        std::ifstream f(path);
        if (!f) {
            std::cout << "ERROR::SHADER::FILE_NOT_FOUND: " << path << std::endl;
            return "";
        }

        std::stringstream out;
        std::string line;
        fs::path dir = fs::path(path).parent_path();

        while (std::getline(f, line)) {
            if (line.rfind("#include", 0) == 0) {
                size_t start = line.find('"');
                size_t end   = line.rfind('"');
                if (start == std::string::npos || end == std::string::npos || end <= start) {
                    std::cout << "ERROR::SHADER::BAD_INCLUDE_LINE: " << line << std::endl;
                    continue;
                }
                std::string included = line.substr(start + 1, end - start - 1);
                out << preprocessImpl((dir / included).string(), seen) << "\n";
            } else {
                out << line << "\n";
            }
        }

        return out.str();
    }
};
#endif