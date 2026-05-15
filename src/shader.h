#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

class Shader{
    public:
        unsigned int ID; //the program ID

        //constrcutor reads and builds the shader
        Shader(const char* vertexPath, const char* fragmentPath) {
            // 1. retrieve the vertex/fragment source code from filePath
            std::string vertexCode;
            std::string fragmentCode;
            std::ifstream vShaderFile;
            std::ifstream fShaderFile;
            // ensure ifstream objects can throw exceptions:
            vShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
            fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
            try 
            {
                // open files
                vShaderFile.open(vertexPath);
                fShaderFile.open(fragmentPath);
                std::stringstream vShaderStream, fShaderStream;
                // read file's buffer contents into streams
                vShaderStream << vShaderFile.rdbuf();
                fShaderStream << fShaderFile.rdbuf();		
                // close file handlers
                vShaderFile.close();
                fShaderFile.close();
                // convert stream into string
                vertexCode   = vShaderStream.str();
                fragmentCode = fShaderStream.str();		
            }
            catch(std::ifstream::failure e)
            {
                std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
            }
            const char* vShaderCode = vertexCode.c_str();
            const char* fShaderCode = fragmentCode.c_str();

            //2.Compile Shaders
            unsigned int vertex, fragment;
            int success;
            char infolog[512];

            //vertex Shader
            vertex = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertex, 1, &vShaderCode, NULL);
            glCompileShader(vertex);
            glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
            if(!success){
                glGetShaderInfoLog(vertex, 512, NULL, infolog);
                std::cout<<"ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"<<infolog<<std::endl;
            }

            //fragment Shader
            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragment, 1, &fShaderCode, NULL);
            glCompileShader(fragment);
            glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
            if(!success){
                glGetShaderInfoLog(fragment, 512, NULL, infolog);
                std::cout<<"ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"<<infolog<<std::endl;
            }

            //Shader Program
            ID = glCreateProgram();
            glAttachShader(ID, vertex);
            glAttachShader(ID, fragment);
            glLinkProgram(ID);
            //print linking errors if any
            glGetProgramiv(ID,GL_LINK_STATUS, &success);
            if(!success){
                glGetProgramInfoLog(ID, 512, NULL, infolog);
                std::cout<<"ERROR::SHADER::PROGRAM::LINKING_FAILED\n"<<infolog<<std::endl;
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

};
#endif