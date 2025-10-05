#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// --- Variáveis Globais ---
vec3 cameraPos   = {0.0f, 0.0f,  8.0f};
vec3 cameraFront = {0.0f, 0.0f, -1.0f};
vec3 cameraUp    = {0.0f, 1.0f,  0.0f};

int   firstMouse = 1;
float yaw   = -90.0f;
float pitch =   0.0f;
float lastX =  800.0f / 2.0f;
float lastY =  600.0f / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Novo: controle de janela/FOV/scroll/toggle de captura
int   winW = 1280, winH = 720;   // tamanho inicial
float fovDeg = 45.0f;            // FOV atual (zoom)
int   mouseCaptured = 1;         // 1=captura ativa
int   togglePressed = 0;         // antirrepique da tecla C

// --- Protótipos ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

void generateSphere(float radius, int sectorCount, int stackCount, 
                    float** vertices, unsigned int* vertexCount, 
                    unsigned int** indices, unsigned int* indexCount);
void generateRing(float innerR, float outerR, int segments,
                  float** vertices, unsigned int* vertexCount,
                  unsigned int** indices, unsigned int* indexCount);

char* loadShaderSource(const char* filePath);
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath);
GLuint loadTexture2D(const char* path);

// --- Estrutura para planetas ---
typedef struct {
    const char* name;
    float orbitRadius;     // distância ao Sol
    float orbitSpeedDeg;   // graus/seg (fictício para demo)
    float axialTiltDeg;    // inclinação do eixo (0 = desligado)
    float spinDeg;         // rotação diária (graus/s; use negativo p/ sentido oposto)
    float scale;           // tamanho relativo
    GLuint texture;        // textura
    float orbitInclDeg;    // inclinação do plano orbital (opcional)
} Planet;

// Desenha um planeta; retorna 'model' em outModel (para anexos como anéis).
static void draw_planet(
    const Planet* p,
    mat4 parentModel,          // sem 'const' por causa do glm_mul
    GLuint shader,
    GLuint vao,
    GLsizei indexCount,
    float t,
    mat4 projection,
    mat4 view,
    vec3 lightPos,
    vec3 cameraPos,
    mat4 outModel              // pode ser NULL
) {
    glUseProgram(shader);

    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, (float*)projection);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, (float*)view);
    glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, (float*)lightPos);
    glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, (float*)cameraPos);

    // ----- ORBITA -----
    mat4 orbit; glm_mat4_identity(orbit);
    if (p->orbitInclDeg != 0.0f)
        glm_rotate(orbit, glm_rad(p->orbitInclDeg), (vec3){1.0f, 0.0f, 0.0f});
    float ang = t * glm_rad(p->orbitSpeedDeg);
    vec3 pos = { cosf(ang) * p->orbitRadius, 0.0f, sinf(ang) * p->orbitRadius };
    glm_translate(orbit, pos);

    // ----- LOCAL (lift, tilt, spin, escala) -----
    mat4 local; glm_mat4_identity(local);
    glm_rotate(local, glm_rad(+90.0f), (vec3){1.0f, 0.0f, 0.0f});            // lift
    if (p->axialTiltDeg != 0.0f)
        glm_rotate(local, glm_rad(p->axialTiltDeg), (vec3){0.0f, 0.0f, 1.0f}); // tilt opcional
    float spin = t * glm_rad(p->spinDeg);
    glm_rotate(local, spin, (vec3){0.0f, 0.0f, 1.0f});                      // spin em Z
    glm_scale(local, (vec3){p->scale, p->scale, p->scale});

    // ----- model = parent * orbit * local -----
    mat4 model, tmp;
    glm_mul(parentModel, orbit, tmp);
    glm_mul(tmp, local, model);

    if (outModel) glm_mat4_copy(model, outModel);

    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, (float*)model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p->texture);
    glUniform1i(glGetUniformLocation(shader, "ourTexture"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

int main(void)
{
    // --- Inicialização ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Sistema Solar v1.6 (Zoom + Resize + Toggle Mouse)", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);                 // <- zoom no scroll
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    // começa capturado
    mouseCaptured = 1;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Shaders ---
    unsigned int objectShaderProgram = createShaderProgram("object_vertex.glsl", "object_fragment.glsl");
    unsigned int lightShaderProgram  = createShaderProgram("light_vertex.glsl",  "light_fragment.glsl");

    // --- Geometria (Esfera) ---
    float* sphereVerts; unsigned int sphereVCount;
    unsigned int* sphereIdx; unsigned int sphereICount;
    generateSphere(1.0f, 48, 24, &sphereVerts, &sphereVCount, &sphereIdx, &sphereICount);

    unsigned int sphereVBO, sphereEBO, sphereVAO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVCount * 8 * sizeof(float), sphereVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereICount * sizeof(unsigned int), sphereIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    free(sphereVerts);
    free(sphereIdx);

    // --- Anel (para Saturno) ---
    float* ringVerts; unsigned int ringVCount;
    unsigned int* ringIdx; unsigned int ringICount;
    generateRing(1.0f, 2.0f, 128, &ringVerts, &ringVCount, &ringIdx, &ringICount);

    unsigned int ringVBO, ringEBO, ringVAO;
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    glGenBuffers(1, &ringEBO);

    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, ringVCount * 8 * sizeof(float), ringVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ringEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ringICount * sizeof(unsigned int), ringIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    free(ringVerts);
    free(ringIdx);

    // --- Texturas ---
    stbi_set_flip_vertically_on_load(1);
    GLuint texSun    = loadTexture2D("sol.jpg");
    GLuint texMerc   = loadTexture2D("mercurio.jpg");
    GLuint texVenus  = loadTexture2D("venus.jpg");
    GLuint texEarth  = loadTexture2D("terra.jpg");
    GLuint texMars   = loadTexture2D("marte.jpg");
    GLuint texJup    = loadTexture2D("jupiter.jpg");
    GLuint texSat    = loadTexture2D("saturno.jpg");
    GLuint texUra    = loadTexture2D("urano.jpg");
    GLuint texNep    = loadTexture2D("netuno.jpg");
    GLuint texSatRings = loadTexture2D("saturno_aneis.png");

    // --- Planetas (valores “de jogo”) ---
    Planet mercurio = {"Mercurio",  1.10f,  55.0f,  0.0f, 140.0f, 0.10f, texMerc,  7.0f};
    Planet venus    = {"Venus",     1.70f,  43.0f,  0.0f, -30.0f, 0.13f, texVenus, 3.4f};
    Planet terra    = {"Terra",     2.50f,  20.0f,  0.0f, -80.0f, 0.25f, texEarth, 0.0f};
    Planet marte    = {"Marte",     3.40f,  16.0f,  0.0f,  80.0f, 0.18f, texMars,  1.9f};
    Planet jupiter  = {"Jupiter",   4.90f,  10.0f,  0.0f, 250.0f, 0.60f, texJup,   1.3f};
    Planet saturno  = {"Saturno",   6.20f,   8.0f,  0.0f, 220.0f, 0.55f, texSat,   2.5f};
    Planet urano    = {"Urano",     7.40f,   6.0f,  0.0f,-150.0f, 0.45f, texUra,   0.8f};
    Planet netuno   = {"Netuno",    8.40f,   5.0f,  0.0f, 180.0f, 0.42f, texNep,   1.8f};

    // --- LOOP ---
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 projection, view;
        glm_perspective(glm_rad(fovDeg), (float)winW / (float)winH, 0.1f, 200.0f, projection);
        vec3 center; glm_vec3_add(cameraPos, cameraFront, center);
        glm_lookat(cameraPos, center, cameraUp, view);

        vec3 lightPos = {0.0f, 0.0f, 0.0f};

        // --- SOL ---
        glUseProgram(lightShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "projection"), 1, GL_FALSE, (float*)projection);
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "view"), 1, GL_FALSE, (float*)view);

        mat4 sunModel;
        glm_mat4_identity(sunModel);
        glm_translate(sunModel, lightPos);
        glm_rotate(sunModel, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f}); // ajuste de textura se necessário
        glm_scale(sunModel, (vec3){0.7f, 0.7f, 0.7f});
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "model"), 1, GL_FALSE, (float*)sunModel);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSun);
        glUniform1i(glGetUniformLocation(lightShaderProgram, "ourTexture"), 0);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereICount, GL_UNSIGNED_INT, 0);

        // --- PLANETAS ---
        float t = (float)glfwGetTime();
        mat4 I; glm_mat4_identity(I);

        draw_planet(&mercurio, I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);
        draw_planet(&venus,    I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);
        draw_planet(&terra,    I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);
        draw_planet(&marte,    I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);
        draw_planet(&jupiter,  I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);

        // Saturno (captura model para anexar anéis)
        mat4 saturnModel;
        draw_planet(&saturno,  I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, saturnModel);

        // --- ANÉIS DE SATURNO ---
        glUseProgram(objectShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "projection"), 1, GL_FALSE, (float*)projection);
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "view"), 1, GL_FALSE, (float*)view);
        glUniform3fv(glGetUniformLocation(objectShaderProgram, "lightPos"), 1, (float*)lightPos);
        glUniform3fv(glGetUniformLocation(objectShaderProgram, "viewPos"), 1, (float*)cameraPos);

        mat4 modelRings;
        glm_mat4_copy(saturnModel, modelRings);
        // desfaz o lift do planeta para o anel ficar no plano XZ do mundo
        glm_rotate(modelRings, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f});
        float ringScale = 0.55f * 2.8f; // ajuste visual
        glm_scale(modelRings, (vec3){ringScale, ringScale, ringScale});
        glUniformMatrix4fv(glGetUniformLocation(objectShaderProgram, "model"), 1, GL_FALSE, (float*)modelRings);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSatRings);
        glUniform1i(glGetUniformLocation(objectShaderProgram, "ourTexture"), 0);
        glDisable(GL_CULL_FACE); // ver anel por cima e por baixo
        glBindVertexArray(ringVAO);
        glDrawElements(GL_TRIANGLES, ringICount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);

        draw_planet(&urano,    I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);
        draw_planet(&netuno,   I, objectShaderProgram, sphereVAO, sphereICount, t, projection, view, lightPos, cameraPos, NULL);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Encerramento simples (OpenGL será limpo pelo SO; adicione glDelete* se desejar)
    glfwTerminate();
    return 0;
}

// --- Auxiliares ---
void processInput(GLFWwindow *window){
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

    float cameraSpeed = 6.0f * deltaTime;
    vec3 v;

    // WASD movimenta a câmera
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){ glm_vec3_scale(cameraFront, cameraSpeed, v); glm_vec3_add(cameraPos, v, cameraPos); }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){ glm_vec3_scale(cameraFront, cameraSpeed, v); glm_vec3_sub(cameraPos, v, cameraPos); }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){ glm_vec3_cross(cameraFront, cameraUp, v); glm_vec3_normalize(v); glm_vec3_scale(v, cameraSpeed, v); glm_vec3_sub(cameraPos, v, cameraPos); }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){ glm_vec3_cross(cameraFront, cameraUp, v); glm_vec3_normalize(v); glm_vec3_scale(v, cameraSpeed, v); glm_vec3_add(cameraPos, v, cameraPos); }

    // Toggle captura do mouse (tecla C)
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!togglePressed) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            togglePressed = 1;
        }
    } else {
        togglePressed = 0;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    if (!mouseCaptured) return;   // não atualiza câmera com o mouse livre (útil para redimensionar)
    if (firstMouse){ lastX = (float)xpos; lastY = (float)ypos; firstMouse = 0; }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity; 
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    vec3 front;
    front[0] = cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
    front[1] = sinf(glm_rad(pitch));
    front[2] = sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));
    glm_normalize_to(front, cameraFront);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    // rolando para cima diminui FOV (aproxima), para baixo aumenta (afasta)
    fovDeg -= (float)yoffset * 2.0f;
    if (fovDeg < 20.0f) fovDeg = 20.0f;
    if (fovDeg > 80.0f) fovDeg = 80.0f;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    if (height == 0) return;
    winW = width;
    winH = height;
    glViewport(0, 0, width, height);
}

char* loadShaderSource(const char* filePath){
    FILE* file = fopen(filePath, "rb");
    if (!file){ printf("Falha ao abrir o arquivo do shader: %s\n", filePath); return NULL; }
    fseek(file, 0, SEEK_END); long length = ftell(file); fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file); fclose(file);
    buffer[length] = '\0'; return buffer;
}

unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath){
    char* vertexSource = loadShaderSource(vertexPath);
    char* fragmentSource = loadShaderSource(fragmentPath);

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, (const char * const*)&vertexSource, NULL);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, (const char * const*)&fragmentSource, NULL);
    glCompileShader(fs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);

    glDeleteShader(vs); glDeleteShader(fs);
    free(vertexSource); free(fragmentSource);
    return prog;
}

GLuint loadTexture2D(const char* path){
    GLuint id; glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int w,h,n; unsigned char *data = stbi_load(path, &w, &h, &n, 0);
    if (data){
        GLenum fmt = (n == 4 ? GL_RGBA : GL_RGB);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        printf("Falha ao carregar textura: %s\n", path);
    }
    stbi_image_free(data);
    return id;
}

// Esfera
void generateSphere(float radius, int sectorCount, int stackCount, 
                    float** vertices, unsigned int* vertexCount, 
                    unsigned int** indices, unsigned int* indexCount){
    *vertexCount = (sectorCount + 1) * (stackCount + 1);
    float* sphereVertices = (float*)malloc(*vertexCount * 8 * sizeof(float));
    float lengthInv = 1.0f / radius;
    float sectorStep = 2.0f * (float)M_PI / sectorCount;
    float stackStep  = (float)M_PI / stackCount;
    int vertexIndex = 0;
    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = (float)M_PI / 2.0f - i * stackStep; // i=0 -> polo norte
        float xy = radius * cosf(stackAngle);
        float z  = radius * sinf(stackAngle);
        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * sectorStep;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            // pos
            sphereVertices[vertexIndex++] = x;
            sphereVertices[vertexIndex++] = y;
            sphereVertices[vertexIndex++] = z;
            // normal
            sphereVertices[vertexIndex++] = x * lengthInv;
            sphereVertices[vertexIndex++] = y * lengthInv;
            sphereVertices[vertexIndex++] = z * lengthInv;
            // uv
            float s = (float)j / sectorCount;
            float t = (float)i / stackCount; // i=0 (norte) -> t=0 ; STB já está flipando a imagem
            sphereVertices[vertexIndex++] = s;
            sphereVertices[vertexIndex++] = t;
        }
    }
    *vertices = sphereVertices;

    *indexCount = stackCount * sectorCount * 6;
    unsigned int* sphereIndices = (unsigned int*)malloc(*indexCount * sizeof(unsigned int));
    int index = 0;
    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                sphereIndices[index++] = k1; 
                sphereIndices[index++] = k2; 
                sphereIndices[index++] = k1 + 1;
            }
            if (i != (stackCount - 1)) {
                sphereIndices[index++] = k1 + 1; 
                sphereIndices[index++] = k2; 
                sphereIndices[index++] = k2 + 1;
            }
        }
    }
    *indices = sphereIndices;
}

// Anel (malha em XZ, normal em +Y)
void generateRing(float innerR, float outerR, int segments,
                  float** vertices, unsigned int* vertexCount,
                  unsigned int** indices, unsigned int* indexCount){
    if (segments < 3) segments = 3;
    int rings = 2;
    *vertexCount = (unsigned int)(segments * rings);
    float* ringVerts = (float*)malloc((*vertexCount) * 8 * sizeof(float));

    int vid = 0;
    for (int i = 0; i < segments; ++i){
        float a = (float)i / (float)segments * 2.0f * (float)M_PI;
        float ca = cosf(a), sa = sinf(a);

        // outer
        float xo = outerR * ca;
        float zo = outerR * sa;
        ringVerts[vid++] = xo; ringVerts[vid++] = 0.0f; ringVerts[vid++] = zo;
        ringVerts[vid++] = 0.0f; ringVerts[vid++] = 1.0f; ringVerts[vid++] = 0.0f;
        ringVerts[vid++] = 1.0f; ringVerts[vid++] = (float)i/(float)segments;

        // inner
        float xi = innerR * ca;
        float zi = innerR * sa;
        ringVerts[vid++] = xi; ringVerts[vid++] = 0.0f; ringVerts[vid++] = zi;
        ringVerts[vid++] = 0.0f; ringVerts[vid++] = 1.0f; ringVerts[vid++] = 0.0f;
        ringVerts[vid++] = 0.0f; ringVerts[vid++] = (float)i/(float)segments;
    }
    *vertices = ringVerts;

    *indexCount = (unsigned int)(segments * 6);
    unsigned int* ringIndices = (unsigned int*)malloc((*indexCount) * sizeof(unsigned int));
    int iid = 0;
    for (int i = 0; i < segments; ++i){
        unsigned int outer_i = (unsigned int)(i * 2);
        unsigned int inner_i = outer_i + 1;
        unsigned int outer_n = (unsigned int)(((i + 1) % segments) * 2);
        unsigned int inner_n = outer_n + 1;

        ringIndices[iid++] = outer_i; ringIndices[iid++] = inner_i; ringIndices[iid++] = outer_n;
        ringIndices[iid++] = inner_i; ringIndices[iid++] = inner_n; ringIndices[iid++] = outer_n;
    }
    *indices = ringIndices;
}
