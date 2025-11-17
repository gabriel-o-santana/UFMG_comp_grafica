#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;

void main()
{

    float ambientStrength = 0.3; // Luz de preenchimento
    vec3 ambient = ambientStrength * lightColor;


    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos); // Direção da luz
    
    // Calcula o ângulo de incidência (quão de frente a face está para a luz)
    float diff = max(dot(norm, lightDir), 0.0); 
    vec3 diffuse = diff * lightColor;

    // Cor final = (Ambiente + Difusa) * Cor do Objeto
    vec3 result = (ambient + diffuse) * objectColor;
    FragColor = vec4(result, 1.0);
}