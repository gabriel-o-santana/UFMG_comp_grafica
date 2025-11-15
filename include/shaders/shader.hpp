#pragma once

#include "glad.h"
#include "glfw3.h"
#include <string>
#include <iostream>

#include <glm/glm.hpp>

class Shader {
    public:
    unsigned int programID;
    std::string vertexFile;
    std::string fragmentFile;

    long fragmentModTimeOnLoad;

    Shader();
    void Unload();
    void ReloadFromFile();
    static Shader LoadShader(std::string fileVertexShader, std::string fileFragmentShader);
    
    // Ativa o shader
    void use() const;
    // Funções para facilitar o envio de dados para o shader
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
    // --------------------------------------------------

    private:
    static bool CompileShader(unsigned int shaderId, char(&infoLog)[512]);
    static bool LinkProgram(unsigned int programID, char(&infoLog)[512]);
};