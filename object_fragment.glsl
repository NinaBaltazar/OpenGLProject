#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D ourTexture;

void main()
{
    // Iluminação Ambiente (uma luz fraca para o lado escuro não ser totalmente preto)
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);

    // Iluminação Difusa (a luz direta do Sol que cria o "dia")
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    
    // Combina as luzes e aplica a textura
    vec3 lighting = (ambient + diffuse);
    FragColor = texture(ourTexture, TexCoord) * vec4(lighting, 1.0);
}