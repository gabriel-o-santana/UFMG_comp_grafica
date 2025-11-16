#include "display/game_window.hpp"
#include "shaders/shader.hpp"
#include <iostream>
#include <vector>
#include <cmath>

// GLM - Matemática para 3D
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- ESTRUTURAS ---

struct Boid {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 forwardDirection; // Armazena a última direção válida
    float wingAngle;            // Estado da animação da asa

    Boid() {
        position = glm::vec3(5.0f, 10.0f, 0.0f); 
        velocity = glm::vec3(0.0f, 0.0f, 0.0f);  
        forwardDirection = glm::vec3(0.0f, 0.0f, 1.0f); // Padrão: olhando para +Z
        wingAngle = 0.0f;
    }
};

// --- GLOBAIS ---
Shader s;
Boid myBoid;

// Geometria
unsigned int VAO_Floor, VBO_Floor;
unsigned int VAO_Cone, VBO_Cone;
int coneVertexCount = 0;
unsigned int VAO_Pyramid, VBO_Pyramid; 
unsigned int VAO_Grid, VBO_Grid;
int gridVertexCount = 0;

// Tempo
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Tela
const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// --- MATEMÁTICA DE ORIENTAÇÃO ---
glm::mat4 calculateOrientation(glm::vec3 position, glm::vec3 forwardVector) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    if (glm::length(forwardVector) < 0.001f) {
        forwardVector = glm::vec3(0.0f, 0.0f, 1.0f); 
    }

    glm::vec3 forward = glm::normalize(forwardVector);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (abs(glm::dot(forward, up)) > 0.99f) 
        up = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 right = glm::normalize(glm::cross(up, forward));
    glm::vec3 realUp = glm::cross(forward, right);

    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(right, 0.0f);
    rotation[1] = glm::vec4(realUp, 0.0f);
    rotation[2] = glm::vec4(forward, 0.0f); 
    rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return model * rotation;
}

// --- CRIAÇÃO DE GEOMETRIA ---

void CreateCommonGeometry() {
    // 1. CHÃO
    float size = 200.0f;
    float floorVertices[] = {
         size, 0.0f,  size,  -size, 0.0f,  size,  -size, 0.0f, -size,
         size, 0.0f,  size,  -size, 0.0f, -size,   size, 0.0f, -size
    };
    glGenVertexArrays(1, &VAO_Floor); glGenBuffers(1, &VBO_Floor);
    glBindVertexArray(VAO_Floor); glBindBuffer(GL_ARRAY_BUFFER, VBO_Floor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 2. GRID
    std::vector<float> gridV;
    float step = 10.0f;
    for (float i = -size; i <= size; i += step) {
        gridV.push_back(i); gridV.push_back(0.05f); gridV.push_back(-size);
        gridV.push_back(i); gridV.push_back(0.05f); gridV.push_back(size);
        gridV.push_back(-size); gridV.push_back(0.05f); gridV.push_back(i);
        gridV.push_back(size);  gridV.push_back(0.05f); gridV.push_back(i);
    }
    gridVertexCount = gridV.size() / 3;
    glGenVertexArrays(1, &VAO_Grid); glGenBuffers(1, &VBO_Grid);
    glBindVertexArray(VAO_Grid); glBindBuffer(GL_ARRAY_BUFFER, VBO_Grid);
    glBufferData(GL_ARRAY_BUFFER, gridV.size() * sizeof(float), gridV.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 3. PIRÂMIDE GENÉRICA
    float pyramidVertices[] = {
        -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f, 0.5f, 0.0f, 
        -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 
         0.5f, -0.5f, 0.0f,  0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 
         0.0f,  0.5f, 0.0f, -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f  
    };
    glGenVertexArrays(1, &VAO_Pyramid); glGenBuffers(1, &VBO_Pyramid);
    glBindVertexArray(VAO_Pyramid); glBindBuffer(GL_ARRAY_BUFFER, VBO_Pyramid);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 4. CONE
    std::vector<float> coneV;
    int segments = 32;
    float radius = 4.0f, height = 12.0f;
    for(int i=0; i<segments; i++) {
        float ang = (float)i/segments * 6.2831f;
        float nextAng = (float)(i+1)/segments * 6.2831f;
        float x1 = cos(ang)*radius, z1 = sin(ang)*radius;
        float x2 = cos(nextAng)*radius, z2 = sin(nextAng)*radius;
        coneV.insert(coneV.end(), {0, height, 0, x1, 0, z1, x2, 0, z2});
        coneV.insert(coneV.end(), {0, 0, 0, x2, 0, z2, x1, 0, z1});
    }
    coneVertexCount = coneV.size() / 3;
    glGenVertexArrays(1, &VAO_Cone); glGenBuffers(1, &VBO_Cone);
    glBindVertexArray(VAO_Cone); glBindBuffer(GL_ARRAY_BUFFER, VBO_Cone);
    glBufferData(GL_ARRAY_BUFFER, coneV.size() * sizeof(float), coneV.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// --- DESENHO DO BOID ---
void DrawBoidParts(const Boid& boid, glm::mat4 baseMatrix) {
    glBindVertexArray(VAO_Pyramid);
    glm::mat4 model;

    // 1. CORPO
    model = glm::scale(baseMatrix, glm::vec3(0.5f, 0.5f, 1.5f)); 
    s.setMat4("model", model);
    s.setVec3("objectColor", 1.0f, 1.0f, 0.0f); // Amarelo
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // 2. CABEÇA (Vermelha)
    model = glm::translate(baseMatrix, glm::vec3(0.0f, 0.0f, 0.8f)); 
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.5f));
    s.setMat4("model", model);
    s.setVec3("objectColor", 1.0f, 0.0f, 0.0f); // Vermelho
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // --- ANIMAÇÃO ---
    float wingRot = sin(boid.wingAngle) * 30.0f;

    // 3. ASA ESQUERDA
    model = glm::translate(baseMatrix, glm::vec3(-0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0,0,1)); 
    s.setMat4("model", model);
    s.setVec3("objectColor", 1.0f, 1.0f, 0.5f);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // 4. ASA DIREITA
    model = glm::translate(baseMatrix, glm::vec3(0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(-wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0,0,1));
    s.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 12);
}

// --- LÓGICA PRINCIPAL (Update) ---

void GameWindow::Update() {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glm::vec3 inputVelocity = glm::vec3(0.0f);
    float baseSpeed = 10.0f; 

    if (glfwGetKey(windowHandle, GLFW_KEY_W) == GLFW_PRESS) inputVelocity.z -= baseSpeed; 
    if (glfwGetKey(windowHandle, GLFW_KEY_S) == GLFW_PRESS) inputVelocity.z += baseSpeed; 
    if (glfwGetKey(windowHandle, GLFW_KEY_A) == GLFW_PRESS) inputVelocity.x -= baseSpeed; 
    if (glfwGetKey(windowHandle, GLFW_KEY_D) == GLFW_PRESS) inputVelocity.x += baseSpeed; 
    if (glfwGetKey(windowHandle, GLFW_KEY_Q) == GLFW_PRESS) inputVelocity.y += baseSpeed; 
    if (glfwGetKey(windowHandle, GLFW_KEY_E) == GLFW_PRESS) inputVelocity.y -= baseSpeed; 

    myBoid.velocity = inputVelocity;

    if (glm::length(myBoid.velocity) > 0.1f) {
        myBoid.forwardDirection = glm::normalize(myBoid.velocity);
    } 

    myBoid.position += myBoid.velocity * deltaTime; 

    // VELOCIDADE DA ASA CONSTANTE
    myBoid.wingAngle += 15.0f * deltaTime;

    s.ReloadFromFile();
}

// --- RENDERIZAÇÃO ---

void GameWindow::Render() {
    glClearColor(0.6f, 0.85f, 0.95f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
    s.use();

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
    s.setMat4("projection", projection);

    glm::mat4 view = glm::lookAt(glm::vec3(0, 40, 60), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    s.setMat4("view", view);

    // 1. Chão
    glm::mat4 model = glm::mat4(1.0f);
    s.setMat4("model", model);
    s.setVec3("objectColor", 0.2f, 0.4f, 0.2f); 
    glBindVertexArray(VAO_Floor); glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2. Grid
    s.setVec3("objectColor", 0.0f, 0.0f, 0.0f); 
    glBindVertexArray(VAO_Grid); glDrawArrays(GL_LINES, 0, gridVertexCount);

    // 3. Torre
    s.setVec3("objectColor", 0.6f, 0.6f, 0.7f); 
    glBindVertexArray(VAO_Cone); glDrawArrays(GL_TRIANGLES, 0, coneVertexCount);

    // 4. Boid
    glm::mat4 boidMatrix = calculateOrientation(myBoid.position, myBoid.forwardDirection);
    DrawBoidParts(myBoid, boidMatrix);

    // Debug UI
    ImGui::Begin("Debug");
    ImGui::Text("Pos: %.1f %.1f %.1f", myBoid.position.x, myBoid.position.y, myBoid.position.z);
    ImGui::End();

    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(windowHandle); glfwPollEvents();
}

void GameWindow::Initialize() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

void GameWindow::LoadContent() {
    glfwSetFramebufferSizeCallback(windowHandle, FramebufferSizeCallback);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui_ImplGlfw_InitForOpenGL(windowHandle, true); ImGui_ImplOpenGL3_Init("#version 330");
    
    s = Shader::LoadShader("resources/shaders/testing.vs", "resources/shaders/testing.fs");
    CreateCommonGeometry();
    glEnable(GL_DEPTH_TEST);
}

void GameWindow::Unload() {
    glDeleteVertexArrays(1, &VAO_Floor); glDeleteBuffers(1, &VBO_Floor);
    glDeleteVertexArrays(1, &VAO_Grid);  glDeleteBuffers(1, &VBO_Grid);
    glDeleteVertexArrays(1, &VAO_Cone);  glDeleteBuffers(1, &VBO_Cone);
    glDeleteVertexArrays(1, &VAO_Pyramid); glDeleteBuffers(1, &VBO_Pyramid);

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
}