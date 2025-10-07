#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D ourTexture;

void main()
{
    // O Sol não é afetado pela luz, ele simplesmente usa a cor da sua própria textura.
    FragColor = texture(ourTexture, TexCoord);
}