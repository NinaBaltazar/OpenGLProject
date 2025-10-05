#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// --- Variáveis Globais ---
vec3 cameraPos   = {0.0f, 0.0f,  5.0f};
vec3 cameraFront = {0.0f, 0.0f, -1.0f};
vec3 cameraUp    = {0.0f, 1.0f,  0.0f};
int firstMouse = 1;
float yaw   = -90.0f;
float pitch =  0.0f;
float lastX =  800.0f / 2.0;
float lastY =  600.0f / 2.0;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Protótipos de Funções ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);
void generateSphere(float radius, int sectorCount, int stackCount, 
                    float** vertices, unsigned int* vertexCount, 
                    unsigned int** indices, unsigned int* indexCount);
char* loadShaderSource(const char* filePath);
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath);


int main(void)
{
    // --- Inicialização ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Sistema Solar v0.6 - Órbita Corrigida!", NULL, NULL);
    glfwMakeContextCurrent(window);
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }
    glEnable(GL_DEPTH_TEST);

    // --- Compilando Shaders de arquivos ---
    unsigned int objectShaderProgram = createShaderProgram("object_vertex.glsl", "object_fragment.glsl");
    unsigned int lightShaderProgram = createShaderProgram("light_vertex.glsl", "light_fragment.glsl");

    // --- Geração da esfera e buffers ---
    float* vertices;
    unsigned int vertexCount;
    unsigned int* indices;
    unsigned int indexCount;
    generateSphere(1.0f, 36, 18, &vertices, &vertexCount, &indices, &indexCount);

    unsigned int VBO, EBO, sphereVAO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 8 * sizeof(float), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    free(vertices);
    free(indices);

    // --- Carregando Texturas ---
    unsigned int earthTexture, sunTexture;
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(1);
    
    glGenTextures(1, &earthTexture);
    glBindTexture(GL_TEXTURE_2D, earthTexture); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    unsigned char *data = stbi_load("terra.jpg", &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else { printf("Falha ao carregar textura da Terra\n"); }
    stbi_image_free(data);

    glGenTextures(1, &sunTexture);
    glBindTexture(GL_TEXTURE_2D, sunTexture); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    data = stbi_load("sol.jpg", &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else { printf("Falha ao carregar textura do Sol\n"); }
    stbi_image_free(data);
    
    // --- LOOP DE RENDERIZAÇÃO ---
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 projection, view;
        glm_perspective(glm_rad(45.0f), 800.0f / 600.0f, 0.1f, 100.0f, projection);
        vec3 center;
        glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);

        vec3 lightPos = {0.0f, 0.0f, 0.0f};

        // --- 1. DESENHAR O SOL ---
        glUseProgram(lightShaderProgram);
        
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "projection"), 1, GL_FALSE, (float*)projection);
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "view"), 1, GL_FALSE, (float*)view);
        
        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, lightPos);
        glm_rotate(model, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f});
        glm_scale(model, (vec3){0.5f, 0.5f, 0.5f});
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "model"), 1, GL_FALSE, (float*)model);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sunTexture);
        glUniform1i(glGetUniformLocation(lightShaderProgram, "ourTexture"), 0);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        // --- 2. DESENHAR A TERRA ---
        glUseProgram(objectShaderProgram);
        
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "projection"), 1, GL_FALSE, (float*)projection);
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "view"), 1, GL_FALSE, (float*)view);
        glUniform3fv(glGetUniformLocation(objectShaderProgram, "lightPos"), 1, (float*)lightPos);
        glUniform3fv(glGetUniformLocation(objectShaderProgram, "viewPos"), 1, (float*)cameraPos);
        
        // --- CORREÇÃO DEFINITIVA DA TRANSLAÇÃO E ROTAÇÃO ---
        glm_mat4_identity(model);
        // 1. Translação Orbital: Move o "ponto de desenho" para a posição na órbita
        float orbitalAngle = (float)glfwGetTime() * glm_rad(20.0f);
        glm_translate(model, (vec3){cos(orbitalAngle) * 2.5f, 0.0f, sin(orbitalAngle) * 2.5f});

        // 2. Rotação de Correção: "Levanta" a esfera para que os polos fiquem no eixo Y
        glm_rotate(model, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f});
        
        // 3. Rotação Axial (Diária): Agora gira em torno do eixo Y do modelo, que está corrigido
        float axialAngle = (float)glfwGetTime() * glm_rad(80.0f);
        glm_rotate(model, axialAngle, (vec3){0.0f, 1.0f, 0.0f});
        
        // 4. Escala: Define o tamanho da Terra
        glm_scale(model, (vec3){0.25f, 0.25f, 0.25f});
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "model"), 1, GL_FALSE, (float*)model);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earthTexture);
        glUniform1i(glGetUniformLocation(objectShaderProgram, "ourTexture"), 0);

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // --- LIMPEZA ---
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(objectShaderProgram);
    glDeleteProgram(lightShaderProgram);
    glDeleteTextures(1, &earthTexture);
    glDeleteTextures(1, &sunTexture);
    glfwTerminate();
    return 0;
}

// --- Funções Auxiliares (Cole estas DEPOIS da função main) ---
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
    float cameraSpeed = 2.5f * deltaTime;
    vec3 velocity;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        glm_vec3_scale(cameraFront, cameraSpeed, velocity);
        glm_vec3_add(cameraPos, velocity, cameraPos);
    } if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        glm_vec3_scale(cameraFront, cameraSpeed, velocity);
        glm_vec3_sub(cameraPos, velocity, cameraPos);
    } if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        glm_vec3_cross(cameraFront, cameraUp, velocity);
        glm_vec3_normalize(velocity);
        glm_vec3_scale(velocity, cameraSpeed, velocity);
        glm_vec3_sub(cameraPos, velocity, cameraPos);
    } if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        glm_vec3_cross(cameraFront, cameraUp, velocity);
        glm_vec3_normalize(velocity);
        glm_vec3_scale(velocity, cameraSpeed, velocity);
        glm_vec3_add(cameraPos, velocity, cameraPos);
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = 0;
    }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    yaw += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    vec3 front;
    front[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
    front[1] = sin(glm_rad(pitch));
    front[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
    glm_normalize_to(front, cameraFront);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

char* loadShaderSource(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (!file) {
        printf("Falha ao abrir o arquivo do shader: %s\n", filePath);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file);
    fclose(file);
    buffer[length] = '\0';
    return buffer;
}

unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath) {
    char* vertexSource = loadShaderSource(vertexPath);
    char* fragmentSource = loadShaderSource(fragmentPath);

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const char * const*)&vertexSource, NULL);
    glCompileShader(vertexShader);
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, (const char * const*)&fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    free(vertexSource);
    free(fragmentSource);

    return shaderProgram;
}

void generateSphere(float radius, int sectorCount, int stackCount, 
                    float** vertices, unsigned int* vertexCount, 
                    unsigned int** indices, unsigned int* indexCount) {
    *vertexCount = (sectorCount + 1) * (stackCount + 1);
    float* sphereVertices = (float*)malloc(*vertexCount * 8 * sizeof(float));
    float x, y, z, xy, nx, ny, nz, s, t;
    float lengthInv = 1.0f / radius;
    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;
    int vertexIndex = 0;
    for(int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);
        for(int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            sphereVertices[vertexIndex++] = x; sphereVertices[vertexIndex++] = y; sphereVertices[vertexIndex++] = z;
            nx = x * lengthInv; ny = y * lengthInv; nz = z * lengthInv;
            sphereVertices[vertexIndex++] = nx; sphereVertices[vertexIndex++] = ny; sphereVertices[vertexIndex++] = nz;
            s = (float)j / sectorCount; t = (float)i / stackCount;
            sphereVertices[vertexIndex++] = s; sphereVertices[vertexIndex++] = t;
        }
    }
    *vertices = sphereVertices;
    *indexCount = stackCount * sectorCount * 6;
    unsigned int* sphereIndices = (unsigned int*)malloc(*indexCount * sizeof(unsigned int));
    int index = 0;
    int k1, k2;
    for(int i = 0; i < stackCount; ++i) {
        k1 = i * (sectorCount + 1);
        k2 = k1 + sectorCount + 1;
        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if(i != 0) {
                sphereIndices[index++] = k1; sphereIndices[index++] = k2; sphereIndices[index++] = k1 + 1;
            } if(i != (stackCount-1)) {
                sphereIndices[index++] = k1 + 1; sphereIndices[index++] = k2; sphereIndices[index++] = k2 + 1;
            }
        }
    }
    *indices = sphereIndices;
}