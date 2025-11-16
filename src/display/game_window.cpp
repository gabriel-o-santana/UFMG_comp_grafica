#include "display/game_window.hpp"
#include "shaders/shader.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib> 

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

// --- TUNING FINO (STEERING BEHAVIORS) ---
const float PERCEPTION_RADIUS = 15.0f; 
const float SEPARATION_RADIUS = 3.0f;   
const float MAX_SPEED = 12.0f;          
const float MAX_FORCE = 0.5f; // Limite da "força de virada" (quanto menor, mais curvas abertas)

// PESOS (Balanceamento Crítico)
// Goal (Líder) deve ser dominante para evitar que eles se percam
const float WEIGHT_GOAL       = 3.5f; 
const float WEIGHT_SEPARATION = 4.0f; // Alta prioridade para não bater
const float WEIGHT_ALIGNMENT  = 1.2f; 
const float WEIGHT_COHESION   = 0.9f; // Baixei a coesão para evitar "bolas" que ignoram o líder
const float WEIGHT_AVOID_FLOOR= 10.0f; 

// --- ESTRUTURAS ---

struct Boid {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    glm::vec3 forwardDirection; 
    float wingAngle;            
    float wingSpeed;

    Boid(glm::vec3 startPos) {
        position = startPos;
        // Velocidade inicial
        velocity = glm::vec3((float)(rand()%10-5), 0.0f, (float)(rand()%10-5));
        if(glm::length(velocity) < 0.1f) velocity = glm::vec3(0,0,1);
        
        forwardDirection = glm::normalize(velocity);
        wingAngle = (float)(rand() % 100);
        wingSpeed = 15.0f + (float)(rand() % 10);
        acceleration = glm::vec3(0.0f);
    }
};

// --- GLOBAIS ---
Shader s;
Boid leaderBoid(glm::vec3(0.0f, 15.0f, 0.0f)); 
std::vector<Boid> flock; 

// Geometria
unsigned int VAO_Floor, VBO_Floor, VAO_Cone, VBO_Cone, VAO_Pyramid, VBO_Pyramid, VAO_Grid, VBO_Grid;
int coneVertexCount = 0;
int gridVertexCount = 0;

// Tempo
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// --- MATEMÁTICA ---
glm::mat4 calculateOrientation(glm::vec3 position, glm::vec3 forwardVector) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    if (glm::length(forwardVector) < 0.01f || std::isnan(forwardVector.x)) forwardVector = glm::vec3(0.0f, 0.0f, 1.0f); 
    glm::vec3 forward = glm::normalize(forwardVector);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (abs(glm::dot(forward, up)) > 0.99f) up = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(up, forward));
    glm::vec3 realUp = glm::cross(forward, right);
    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(right, 0.0f);
    rotation[1] = glm::vec4(realUp, 0.0f);
    rotation[2] = glm::vec4(forward, 0.0f); 
    rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return model * rotation;
}

// --- GEOMETRIA ---
void CreateCommonGeometry() {
    float size = 200.0f;
    float floorVertices[] = { size, 0.0f, size, -size, 0.0f, size, -size, 0.0f, -size, size, 0.0f, size, -size, 0.0f, -size, size, 0.0f, -size };
    glGenVertexArrays(1, &VAO_Floor); glGenBuffers(1, &VBO_Floor);
    glBindVertexArray(VAO_Floor); glBindBuffer(GL_ARRAY_BUFFER, VBO_Floor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

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

    float pyramidVertices[] = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f };
    glGenVertexArrays(1, &VAO_Pyramid); glGenBuffers(1, &VBO_Pyramid);
    glBindVertexArray(VAO_Pyramid); glBindBuffer(GL_ARRAY_BUFFER, VBO_Pyramid);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    std::vector<float> coneV;
    int segments = 32; float radius = 4.0f, height = 12.0f;
    for(int i=0; i<segments; i++) {
        float ang = (float)i/segments * 6.2831f; float nextAng = (float)(i+1)/segments * 6.2831f;
        float x1 = cos(ang)*radius, z1 = sin(ang)*radius; float x2 = cos(nextAng)*radius, z2 = sin(nextAng)*radius;
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

void DrawBoidParts(const Boid& boid, glm::mat4 baseMatrix, bool isLeader) {
    glBindVertexArray(VAO_Pyramid);
    glm::mat4 model;
    glm::vec3 bodyColor = isLeader ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(1.0f, 1.0f, 0.0f);
    glm::vec3 wingColor = isLeader ? glm::vec3(1.0f, 0.5f, 0.5f) : glm::vec3(1.0f, 1.0f, 0.5f);

    model = glm::scale(baseMatrix, glm::vec3(0.5f, 0.5f, 1.5f)); 
    s.setMat4("model", model); s.setVec3("objectColor", bodyColor); glDrawArrays(GL_TRIANGLES, 0, 12);

    model = glm::translate(baseMatrix, glm::vec3(0.0f, 0.0f, 0.8f)); 
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.5f));
    s.setMat4("model", model); s.setVec3("objectColor", 1.0f, 0.0f, 0.0f); glDrawArrays(GL_TRIANGLES, 0, 12);

    float wingRot = sin(boid.wingAngle) * 30.0f;
    model = glm::translate(baseMatrix, glm::vec3(-0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0,0,1)); 
    s.setMat4("model", model); s.setVec3("objectColor", wingColor); glDrawArrays(GL_TRIANGLES, 0, 12);

    model = glm::translate(baseMatrix, glm::vec3(0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(-wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0,0,1));
    s.setMat4("model", model); glDrawArrays(GL_TRIANGLES, 0, 12);
}

// --- LÓGICA STEERING BEHAVIORS (O CÉREBRO) ---

// Função mágica de Reynolds: Calcula força para atingir um alvo
glm::vec3 SteerTowards(Boid& b, glm::vec3 target) {
    glm::vec3 desired = target - b.position; // Vetor para o alvo
    float dist = glm::length(desired);
    
    if (dist == 0) return glm::vec3(0.0f);

    desired = glm::normalize(desired);

    // Chegada Suave (Arrival): Se estiver perto, desacelera
    if (dist < 10.0f) {
        desired *= (MAX_SPEED * (dist / 10.0f));
    } else {
        desired *= MAX_SPEED;
    }

    glm::vec3 steer = desired - b.velocity; // Fórmula de Reynolds: Steering = Desired - Velocity
    
    // Limita a força para eles não virarem instantaneamente (curva suave)
    if (glm::length(steer) > MAX_FORCE) {
        steer = glm::normalize(steer) * MAX_FORCE;
    }
    return steer;
}

void UpdateFlock(float dt) {
    leaderBoid.wingAngle += leaderBoid.wingSpeed * dt;

    for (auto& b : flock) {
        b.acceleration = glm::vec3(0.0f);
        
        glm::vec3 sep(0.0f);
        glm::vec3 ali(0.0f);
        glm::vec3 coh(0.0f);
        int count = 0;

        for (const auto& other : flock) {
            if (&b == &other) continue;
            
            float d = glm::distance(b.position, other.position);
            
            if (d > 0 && d < PERCEPTION_RADIUS) {
                // Alinhamento (Média das velocidades)
                ali += other.velocity;
                
                // Coesão (Média das posições)
                coh += other.position;
                
                // Separação (Vetor contrário se muito perto)
                if (d < SEPARATION_RADIUS) {
                    glm::vec3 push = glm::normalize(b.position - other.position);
                    sep += push / d; // Peso pela distância
                }
                count++;
            }
        }

        if (count > 0) {
            // Média Alinhamento + Steering
            ali /= (float)count;
            ali = glm::normalize(ali) * MAX_SPEED;
            glm::vec3 steerAli = ali - b.velocity;
            if(glm::length(steerAli) > MAX_FORCE) steerAli = glm::normalize(steerAli) * MAX_FORCE;
            
            // Média Coesão (Ir para o centro) + Steering
            coh /= (float)count;
            glm::vec3 steerCoh = SteerTowards(b, coh);

            // Separação (já é vetor de força)
            glm::vec3 steerSep = sep;
            if(glm::length(steerSep) > 0) {
                steerSep = glm::normalize(steerSep) * MAX_SPEED;
                steerSep -= b.velocity;
                if(glm::length(steerSep) > MAX_FORCE) steerSep = glm::normalize(steerSep) * MAX_FORCE;
            }

            b.acceleration += steerSep * WEIGHT_SEPARATION;
            b.acceleration += steerAli * WEIGHT_ALIGNMENT;
            b.acceleration += steerCoh * WEIGHT_COHESION;
        }

        // --- FORÇA DO LÍDER (PRIORIDADE MÁXIMA) ---
        glm::vec3 steerGoal = SteerTowards(b, leaderBoid.position);
        b.acceleration += steerGoal * WEIGHT_GOAL;

        // Evitar Chão
        if (b.position.y < 5.0f) {
            glm::vec3 desired = b.velocity;
            desired.y = MAX_SPEED; // Quer subir com força total
            glm::vec3 steerFloor = desired - b.velocity;
            b.acceleration += steerFloor * WEIGHT_AVOID_FLOOR;
        }

        // Atualiza Física
        b.velocity += b.acceleration * dt;
        // Limita velocidade final (não deixa passar do Max nem ficar abaixo do Min)
        float speed = glm::length(b.velocity);
        if (speed > MAX_SPEED) b.velocity = glm::normalize(b.velocity) * MAX_SPEED;
        // Velocidade mínima para eles não pararem e caírem
        if (speed < 2.0f) b.velocity = glm::normalize(b.velocity) * 2.0f; 

        b.position += b.velocity * dt;
        b.wingAngle += b.wingSpeed * dt;
        
        if (glm::length(b.velocity) > 0.1f)
            b.forwardDirection = glm::normalize(b.velocity);
    }
}

// --- INPUT ---

void ProcessInput(GLFWwindow *window) {
    float leaderSpeed = 8.0f; 
    glm::vec3 inputDirection(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputDirection.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputDirection.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputDirection.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputDirection.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) inputDirection.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) inputDirection.y -= 1.0f;
    
    if (glm::length(inputDirection) > 0.0f) {
        leaderBoid.velocity = glm::normalize(inputDirection) * leaderSpeed;
    } else {
        leaderBoid.velocity *= 0.95f;
    }

    leaderBoid.position += leaderBoid.velocity * deltaTime;
    if (glm::length(leaderBoid.velocity) > 0.1f) 
        leaderBoid.forwardDirection = glm::normalize(leaderBoid.velocity);

    // Adicionar Boids (+)
    static bool btnPlus = false;
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
        if (!btnPlus) {
            flock.push_back(Boid(leaderBoid.position + glm::vec3(rand()%5, rand()%5, rand()%5)));
            btnPlus = true;
        }
    } else btnPlus = false;

    // Remover Boids (-)
    static bool btnMinus = false;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
        if (!btnMinus) {
            if(!flock.empty()) flock.pop_back();
            btnMinus = true;
        }
    } else btnMinus = false;
}

// --- UPDATE & RENDER ---

void GameWindow::Update() {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    ProcessInput(this->windowHandle);
    UpdateFlock(deltaTime);
    s.ReloadFromFile();
}

void GameWindow::Render() {
    glClearColor(0.6f, 0.85f, 0.95f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
    s.use();

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
    s.setMat4("projection", projection);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 40, 80), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    s.setMat4("view", view);

    s.setMat4("model", glm::mat4(1.0f));
    s.setVec3("objectColor", 0.2f, 0.4f, 0.2f); 
    glBindVertexArray(VAO_Floor); glDrawArrays(GL_TRIANGLES, 0, 6);
    
    s.setVec3("objectColor", 0.0f, 0.0f, 0.0f); 
    glBindVertexArray(VAO_Grid); glDrawArrays(GL_LINES, 0, gridVertexCount);

    s.setVec3("objectColor", 0.6f, 0.6f, 0.7f); 
    glBindVertexArray(VAO_Cone); glDrawArrays(GL_TRIANGLES, 0, coneVertexCount);

    glm::mat4 leaderM = calculateOrientation(leaderBoid.position, leaderBoid.forwardDirection);
    DrawBoidParts(leaderBoid, leaderM, true);

    for (const auto& b : flock) {
        glm::mat4 boidM = calculateOrientation(b.position, b.forwardDirection);
        DrawBoidParts(b, boidM, false);
    }

    ImGui::Begin("Debug");
    ImGui::Text("Lider Pos: %.1f %.1f %.1f", leaderBoid.position.x, leaderBoid.position.y, leaderBoid.position.z);
    ImGui::Text("Boids: %d", (int)flock.size());
    ImGui::End();

    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(windowHandle); glfwPollEvents();
}

void GameWindow::Initialize() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

// --- LOAD CONTENT ---
void GameWindow::LoadContent() {
    glfwSetFramebufferSizeCallback(windowHandle, FramebufferSizeCallback);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui_ImplGlfw_InitForOpenGL(windowHandle, true); ImGui_ImplOpenGL3_Init("#version 330");
    
    s = Shader::LoadShader("resources/shaders/testing.vs", "resources/shaders/testing.fs");
    CreateCommonGeometry();
    glEnable(GL_DEPTH_TEST);
    
    leaderBoid.position = glm::vec3(0, 15, 0);

    flock.clear();
    // Spawn inicial de 10 boids
    for(int i = 0; i < 10; i++) {
        float angle = (float)i / 10.0f * 6.28f; 
        glm::vec3 offset(cos(angle)*2.0f, 0.0f, sin(angle)*2.0f);
        flock.push_back(Boid(leaderBoid.position + offset));
    }
}

void GameWindow::Unload() {
    glDeleteVertexArrays(1, &VAO_Floor); glDeleteBuffers(1, &VBO_Floor);
    glDeleteVertexArrays(1, &VAO_Grid);  glDeleteBuffers(1, &VBO_Grid);
    glDeleteVertexArrays(1, &VAO_Cone);  glDeleteBuffers(1, &VBO_Cone);
    glDeleteVertexArrays(1, &VAO_Pyramid); glDeleteBuffers(1, &VBO_Pyramid);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
}