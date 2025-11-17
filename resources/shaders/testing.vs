#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;   // Posição no mundo
out vec3 Normal;    // Normal no mundo

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    // Passa a Posição e a Normal (transformadas) para o fragment shader
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Usa a transposta inversa para transformar a normal (evita distorção com escala)
    Normal = mat3(transpose(inverse(model))) * aNormal;
}