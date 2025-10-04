#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

// Matrizes para a transformação 3D
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Transforma a posição do vértice para o espaço 3D da câmera e da tela
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}