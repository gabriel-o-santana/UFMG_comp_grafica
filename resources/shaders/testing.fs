#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform bool useLighting;

// Novo: cores do céu
const vec3 skyTop    = vec3(0.10, 0.20, 0.45);  // Azul escuro
const vec3 skyBottom = vec3(0.55, 0.75, 0.95);  // Azul claro

void main()
{
    // --- CÉU GRADIENTE ---
    if (!useLighting)
    {
        // Cor pura para sombras e grid
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    // Se o fragmento está "muito longe" acima do horizonte,
    // aplicamos gradiente automático
    if (FragPos.y > 150.0)
    {
        float t = clamp((FragPos.y - 150.0) / 200.0, 0.0, 1.0);
        vec3 skyColor = mix(skyBottom, skyTop, t);
        FragColor = vec4(skyColor, 1.0);
        return;
    }

    // ---------------------------
    // 1. Luz Ambiente
    float ambientStrength = 0.35;
    vec3 ambient = ambientStrength * lightColor;

    // 2. Luz Difusa
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 result = (ambient + diffuse) * objectColor;

    // --- NEBLINA SUAVE (FOG) ---
    float distance = length(FragPos - vec3(0, 40, 60)); // olho aproximado
    float fogAmount = clamp((distance - 50.0) / 300.0, 0.0, 1.0);
    vec3 fogColor = skyBottom;

    result = mix(result, fogColor, fogAmount);

    FragColor = vec4(result, 1.0);
}
