#version 330 core
out vec4 FragColor;

in vec2 vUV;
uniform sampler2D skyTex;

void main() {
    FragColor = texture(skyTex, vUV);
}
