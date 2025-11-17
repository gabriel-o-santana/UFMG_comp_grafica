#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform bool useLighting; // NOVO: Interruptor da luz

void main()
{
    if (!useLighting)
    {
        // Se a luz estiver desligada, usa a cor pura (para sombras e grid)
        FragColor = vec4(objectColor, 1.0);
    }
    else
    {
        // 1. Luz Ambiente
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * lightColor;

        // 2. Luz Difusa
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Cor final
        vec3 result = (ambient + diffuse) * objectColor;
        FragColor = vec4(result, 1.0);
    }
}