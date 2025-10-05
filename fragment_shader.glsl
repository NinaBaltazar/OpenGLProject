#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

// <-- NOVO: A textura que vamos receber do código C
uniform sampler2D ourTexture;

void main()
{
    // A função texture() busca a cor na imagem (ourTexture)
    // usando a coordenada de textura (TexCoord)
    FragColor = texture(ourTexture, TexCoord);
}