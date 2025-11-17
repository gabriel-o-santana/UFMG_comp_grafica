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

// --- TUNING: BOIDS ÁGEIS ---
const float PERCEPTION_RADIUS = 10.0f; 
const float SEPARATION_RADIUS = 2.0f;   
const float MAX_SPEED = 15.0f;          
const float MIN_SPEED = 8.0f;
const float MAX_FORCE = 3.5f;           

// PESOS
const float WEIGHT_SEPARATION = 3.0f;   
const float WEIGHT_ALIGNMENT  = 1.5f;   
const float WEIGHT_COHESION   = 2.5f;   
const float WEIGHT_GOAL       = 5.0f;   
const float WEIGHT_AVOID_FLOOR= 10.0f; 

// --- FÍSICA DO LÍDER (CORRIGIDO E RÁPIDO) ---
const float LEADER_THRUST = 80.0f;    
const float LEADER_MAX_SPEED = 15.0f; 
const float LEADER_DAMPING = 0.96f;   

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
        velocity = glm::vec3((float)(rand()%10-5), 0.0f, (float)(rand()%10-5));
        if(glm::length(velocity) < 0.1f) velocity = glm::vec3(0,0,1);
        velocity = glm::normalize(velocity) * MIN_SPEED;
        
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

// Câmera
int activeCameraMode = 0; 
glm::vec3 flockCenter(0.0f);
glm::vec3 flockAverageVelocity(0.0f, 0.0f, 1.0f); 
const float TOWER_HEIGHT = 80.0f;

glm::vec3 leaderInputDirection(0.0f); // Guarda o input do teclado

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// --- MATEMÁTICA ---
glm::mat4 calculateOrientation(glm::vec3 position, glm::vec3 forwardVector) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    if (glm::length(forwardVector) < 0.01f || std::isnan(forwardVector.x)) {
        forwardVector = glm::vec3(0.0f, 0.0f, 1.0f); 
    }

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

// --- GEOMETRIA (COM NORMAIS) ---
void CreateCommonGeometry() {
    // 1. CHÃO (com normais)
    float size = 200.0f;
    float floorVertices[] = {
         size, 0.0f,  size,  0.0f, 1.0f, 0.0f, -size, 0.0f,  size,  0.0f, 1.0f, 0.0f, -size, 0.0f, -size,  0.0f, 1.0f, 0.0f,
         size, 0.0f,  size,  0.0f, 1.0f, 0.0f, -size, 0.0f, -size,  0.0f, 1.0f, 0.0f,  size, 0.0f, -size,  0.0f, 1.0f, 0.0f
    };
    glGenVertexArrays(1, &VAO_Floor); glGenBuffers(1, &VBO_Floor);
    glBindVertexArray(VAO_Floor); glBindBuffer(GL_ARRAY_BUFFER, VBO_Floor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 2. GRID
    std::vector<float> gridV;
    float step = 10.0f;
    for (float i = -size; i <= size; i += step) {
        gridV.push_back(i); gridV.push_back(0.06f); gridV.push_back(-size);
        gridV.push_back(i); gridV.push_back(0.06f); gridV.push_back(size);
        gridV.push_back(-size); gridV.push_back(0.06f); gridV.push_back(i);
        gridV.push_back(size);  gridV.push_back(0.06f); gridV.push_back(i);
    }
    gridVertexCount = gridV.size() / 3;
    glGenVertexArrays(1, &VAO_Grid); glGenBuffers(1, &VBO_Grid);
    glBindVertexArray(VAO_Grid); glBindBuffer(GL_ARRAY_BUFFER, VBO_Grid);
    glBufferData(GL_ARRAY_BUFFER, gridV.size() * sizeof(float), gridV.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 3. PIRÂMIDE GENÉRICA (com normais)
    float pyramidVertices[] = {
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, -1.0f,  0.5f, -0.5f, 0.0f,  0.0f, 0.0f, -1.0f,  0.0f,  0.5f, 0.0f,  0.0f, 0.0f, -1.0f, 
        -0.5f, -0.5f, 0.0f,  0.0f, -0.87f, 0.5f, 0.5f, -0.5f, 0.0f,  0.0f, -0.87f, 0.5f, 0.0f,  0.0f, 1.0f,  0.0f, -0.87f, 0.5f, 
         0.5f, -0.5f, 0.0f,  0.87f, 0.0f, 0.5f,  0.0f,  0.5f, 0.0f,  0.87f, 0.0f, 0.5f,  0.0f,  0.0f, 1.0f,  0.87f, 0.0f, 0.5f, 
         0.0f,  0.5f, 0.0f, -0.87f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, -0.87f, 0.0f, 0.5f, 0.0f,  0.0f, 1.0f, -0.87f, 0.0f, 0.5f  
    };
    glGenVertexArrays(1, &VAO_Pyramid); glGenBuffers(1, &VBO_Pyramid);
    glBindVertexArray(VAO_Pyramid); glBindBuffer(GL_ARRAY_BUFFER, VBO_Pyramid);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 4. CONE (com normais)
    std::vector<float> coneV;
    int segments = 32; 
    float radius = 15.0f; // Raio largo
    float height = TOWER_HEIGHT;
    float slopeY = radius / height; 
    for(int i=0; i<segments; i++) {
        float ang = (float)i/segments * 6.2831f; 
        float nextAng = (float)(i+1)/segments * 6.2831f;
        float x1 = cos(ang)*radius, z1 = sin(ang)*radius;
        float x2 = cos(nextAng)*radius, z2 = sin(nextAng)*radius;
        glm::vec3 n1 = glm::normalize(glm::vec3(x1, slopeY, z1));
        glm::vec3 n2 = glm::normalize(glm::vec3(x2, slopeY, z2));
        coneV.insert(coneV.end(), {0, height, 0,  0, 1, 0});
        coneV.insert(coneV.end(), {x1, 0, z1,      n1.x, n1.y, n1.z});
        coneV.insert(coneV.end(), {x2, 0, z2,      n2.x, n2.y, n2.z});
        coneV.insert(coneV.end(), {0, 0, 0,  0, -1, 0});
        coneV.insert(coneV.end(), {x2, 0, z2, 0, -1, 0});
        coneV.insert(coneV.end(), {x1, 0, z1, 0, -1, 0});
    }
    coneVertexCount = coneV.size() / 6;
    glGenVertexArrays(1, &VAO_Cone); glGenBuffers(1, &VBO_Cone);
    glBindVertexArray(VAO_Cone); glBindBuffer(GL_ARRAY_BUFFER, VBO_Cone);
    glBufferData(GL_ARRAY_BUFFER, coneV.size() * sizeof(float), coneV.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// --- DESENHO (CORRIGIDO) ---
// Adicionamos 'bool useLighting' para controlar as cores
void DrawBoidParts(const Boid& boid, glm::mat4 baseMatrix, bool isLeader, bool useLighting) {
    glBindVertexArray(VAO_Pyramid);
    glm::mat4 model;
    
    // --- CORPO ---
    if (useLighting) {
        glm::vec3 bodyColor = isLeader ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(1.0f, 1.0f, 0.0f);
        s.setVec3("objectColor", bodyColor);
    }
    model = glm::scale(baseMatrix, glm::vec3(0.5f, 0.5f, 1.5f)); 
    s.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // --- CABEÇA ---
    if (useLighting) {
        s.setVec3("objectColor", 1.0f, 0.0f, 0.0f); // Vermelho
    }
    model = glm::translate(baseMatrix, glm::vec3(0.0f, 0.0f, 0.8f)); 
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.5f));
    s.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // --- ASAS ---
    if (useLighting) {
        glm::vec3 wingColor = isLeader ? glm::vec3(1.0f, 0.5f, 0.5f) : glm::vec3(1.0f, 1.0f, 0.5f);
        s.setVec3("objectColor", wingColor);
    }
    float wingRot = sin(boid.wingAngle) * 30.0f;

    // Asa Esquerda
    model = glm::translate(baseMatrix, glm::vec3(-0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0,0,1)); 
    s.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // Asa Direita
    model = glm::translate(baseMatrix, glm::vec3(0.2f, 0.0f, 0.2f));
    model = glm::rotate(model, glm::radians(-wingRot), glm::vec3(0, 0, 1)); 
    model = glm::scale(model, glm::vec3(1.2f, 0.1f, 0.8f));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0,0,1));
    s.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 12);
}

// --- LÓGICA DE FLOCKING ---

glm::vec3 limitVector(glm::vec3 v, float maxVal) {
    if (glm::length(v) > maxVal) return glm::normalize(v) * maxVal;
    return v;
}

glm::vec3 SteerTowards(Boid& b, glm::vec3 target) {
    glm::vec3 desired = target - b.position; 
    float dist = glm::length(desired);
    if (dist == 0) return glm::vec3(0.0f);

    desired = glm::normalize(desired) * MAX_SPEED;

    glm::vec3 steer = desired - b.velocity; 
    if (glm::length(steer) > MAX_FORCE) {
        steer = glm::normalize(steer) * MAX_FORCE;
    }
    return steer;
}

void UpdateFlock(float dt) {
    // --- FÍSICA DO LÍDER (MOVIMENTO SUAVE E RÁPIDO) ---
    if (glm::length(leaderInputDirection) > 0.0f) {
        leaderBoid.acceleration = glm::normalize(leaderInputDirection) * LEADER_THRUST;
    } else {
        leaderBoid.acceleration = glm::vec3(0.0f);
    }
    leaderBoid.velocity += leaderBoid.acceleration * dt * 5.0f; 
    leaderBoid.velocity *= LEADER_DAMPING; 
    if (glm::length(leaderBoid.velocity) > LEADER_MAX_SPEED) {
        leaderBoid.velocity = glm::normalize(leaderBoid.velocity) * LEADER_MAX_SPEED;
    }
    leaderBoid.position += leaderBoid.velocity * dt;
    if (glm::length(leaderBoid.velocity) > 0.1f) 
        leaderBoid.forwardDirection = glm::normalize(leaderBoid.velocity);
    leaderBoid.wingAngle += leaderBoid.wingSpeed * dt;
    // --- FIM DA FÍSICA DO LÍDER ---


    // --- FÍSICA DO BANDO ---
    glm::vec3 centerSum(0.0f);
    glm::vec3 velocitySum(0.0f);

    for (auto& b : flock) {
        b.acceleration = glm::vec3(0.0f);
        glm::vec3 separation(0.0f), alignment(0.0f), cohesion(0.0f);
        int neighbors = 0;

        for (const auto& other : flock) {
            if (&b == &other) continue;
            float dist = glm::distance(b.position, other.position);
            if (dist < PERCEPTION_RADIUS) {
                cohesion += other.position;
                alignment += other.velocity;
                if (dist < SEPARATION_RADIUS) {
                    glm::vec3 push = b.position - other.position;
                    separation += glm::normalize(push) / (dist * dist + 0.01f);
                }
                neighbors++;
            }
        }

        glm::vec3 steerAli(0.0f), steerCoh(0.0f), steerSep(0.0f);
        if (neighbors > 0) {
            cohesion /= (float)neighbors;
            steerCoh = SteerTowards(b, cohesion);
            alignment /= (float)neighbors;
            alignment = glm::normalize(alignment) * MAX_SPEED;
            steerAli = alignment - b.velocity;
            steerAli = limitVector(steerAli, MAX_FORCE);
            if(glm::length(separation) > 0) {
                separation = glm::normalize(separation) * MAX_SPEED;
                steerSep = separation - b.velocity;
                steerSep = limitVector(steerSep, MAX_FORCE);
            }
        }

        glm::vec3 steerGoal = SteerTowards(b, leaderBoid.position);
        glm::vec3 steerFloor(0.0f);
        if (b.position.y < 5.0f) {
            glm::vec3 desired = b.velocity;
            desired.y = MAX_SPEED;
            steerFloor = desired - b.velocity;
        }

        b.acceleration += steerSep * WEIGHT_SEPARATION;
        b.acceleration += steerAli * WEIGHT_ALIGNMENT;
        b.acceleration += steerCoh * WEIGHT_COHESION;
        b.acceleration += steerGoal * WEIGHT_GOAL;
        b.acceleration += steerFloor * WEIGHT_AVOID_FLOOR;

        b.acceleration = limitVector(b.acceleration, MAX_FORCE);
        b.velocity += b.acceleration * dt * 5.0f; 
        b.velocity = limitVector(b.velocity, MAX_SPEED);
        
        if (glm::length(b.velocity) < MIN_SPEED) 
             b.velocity = glm::normalize(b.velocity) * MIN_SPEED;

        b.position += b.velocity * dt;
        b.wingAngle += b.wingSpeed * dt;
        b.forwardDirection = glm::normalize(b.velocity);
        
        centerSum += b.position;
        velocitySum += b.velocity;
    }

    // Cálculo Final da Média do Bando (Alvo da Câmera)
    if (!flock.empty()) {
        flockCenter = centerSum / (float)flock.size();
        flockAverageVelocity = velocitySum / (float)flock.size();
    } else {
        flockCenter = leaderBoid.position;
        flockAverageVelocity = leaderBoid.velocity;
    }
}

// --- INPUT ---

void ProcessInput(GLFWwindow *window) {
    leaderInputDirection = glm::vec3(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) leaderInputDirection.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) leaderInputDirection.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) leaderInputDirection.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) leaderInputDirection.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) leaderInputDirection.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) leaderInputDirection.y -= 1.0f;
    
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) activeCameraMode = 0;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) activeCameraMode = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) activeCameraMode = 2;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) activeCameraMode = 3;

    static bool btnPlus = false;
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
        if (!btnPlus) {
            flock.push_back(Boid(leaderBoid.position + glm::vec3(rand()%5, rand()%5, rand()%5)));
            btnPlus = true;
        }
    } else btnPlus = false;

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

    s.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
    s.setVec3("lightPos", 0.0f, 150.0f, 100.0f); 

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
    s.setMat4("projection", projection);
    
    glm::mat4 view;
    glm::vec3 center = flockCenter; 
    glm::vec3 up(0.0f, 1.0f, 0.0f); 
    glm::vec3 avgFlockDir = flockAverageVelocity;
    if (glm::length(avgFlockDir) < 0.1f) avgFlockDir = glm::vec3(0, 0, 1); 
    else avgFlockDir = glm::normalize(avgFlockDir);

    switch(activeCameraMode) {
        case 1: { 
            glm::vec3 eye = glm::vec3(0.0f, TOWER_HEIGHT + 2.0f, 0.1f); 
            view = glm::lookAt(eye, center, up);
            break;
        }
        case 2: { 
            float distance = 40.0f;
            float height = 12.0f;
            glm::vec3 eye = center - (avgFlockDir * distance) + glm::vec3(0.0f, height, 0.0f);
            view = glm::lookAt(eye, center, up);
            break;
        }
        case 3: { 
            float distance = 40.0f;
            float height = 5.0f;
            glm::vec3 rightDir = glm::normalize(glm::cross(avgFlockDir, up)); 
            glm::vec3 eye = center + (rightDir * distance) + glm::vec3(0.0f, height, 0.0f);
            view = glm::lookAt(eye, center, up);
            break;
        }
        default: 
        case 0: { 
            glm::vec3 eye = glm::vec3(0, 40, 80);
            view = glm::lookAt(eye, glm::vec3(0,0,0), up);
            break;
        }
    }
    s.setMat4("view", view);

    // --- Desenha o mundo ---
    s.setBool("useLighting", true); 
    s.setMat4("model", glm::mat4(1.0f));
    s.setVec3("objectColor", 0.2f, 0.4f, 0.2f); 
    glBindVertexArray(VAO_Floor); glDrawArrays(GL_TRIANGLES, 0, 6);
    
    s.setVec3("objectColor", 0.6f, 0.6f, 0.7f); 
    glBindVertexArray(VAO_Cone); glDrawArrays(GL_TRIANGLES, 0, coneVertexCount);

    s.setBool("useLighting", false); // Desliga luz para o grid
    s.setVec3("objectColor", 0.0f, 0.0f, 0.0f); 
    glBindVertexArray(VAO_Grid); glDrawArrays(GL_LINES, 0, gridVertexCount);
    // --- Fim do mundo ---


    // --- Desenha os Atores (COM SOMBRAS) ---

    // 1. Sombra do Líder
    glm::vec3 shadowPos = leaderBoid.position;
    shadowPos.y = 0.05f; 
    glm::mat4 shadowMatrix = calculateOrientation(shadowPos, leaderBoid.forwardDirection);
    shadowMatrix = glm::scale(shadowMatrix, glm::vec3(1.0f, 0.05f, 1.0f));
    
    s.setBool("useLighting", false);
    s.setVec3("objectColor", 0.0f, 0.0f, 0.0f); // Sombra preta
    DrawBoidParts(leaderBoid, shadowMatrix, true, false); // NOVO: Passa 'false' para useLighting

    // 2. Líder Real
    s.setBool("useLighting", true); 
    glm::mat4 leaderM = calculateOrientation(leaderBoid.position, leaderBoid.forwardDirection);
    DrawBoidParts(leaderBoid, leaderM, true, true); // NOVO: Passa 'true' para useLighting

    // 3. Sombras e Boids do Bando
    for (const auto& b : flock) {
        // Sombra
        shadowPos = b.position;
        shadowPos.y = 0.05f;
        shadowMatrix = calculateOrientation(shadowPos, b.forwardDirection);
        shadowMatrix = glm::scale(shadowMatrix, glm::vec3(1.0f, 0.05f, 1.0f));
        
        s.setBool("useLighting", false);
        s.setVec3("objectColor", 0.0f, 0.0f, 0.0f);
        DrawBoidParts(b, shadowMatrix, false, false); // NOVO: Passa 'false'

        // Boid Real
        s.setBool("useLighting", true);
        glm::mat4 boidM = calculateOrientation(b.position, b.forwardDirection);
        DrawBoidParts(b, boidM, false, true); // NOVO: Passa 'true'
    }

    // HUD
    ImGui::Begin("Debug");
    std::string camMode;
    switch(activeCameraMode) {
        case 1: camMode = "Torre (1)"; break;
        case 2: camMode = "Atras (2)"; break;
        case 3: camMode = "Lateral (3)"; break;
        default: camMode = "Debug Fixa (0)"; break;
    }
    ImGui::Text("Camera: %s", camMode.c_str());
    ImGui::Text("Boids: %d", (int)flock.size());
    ImGui::Text("Lider Pos: %.1f %.1f %.1f", leaderBoid.position.x, leaderBoid.position.y, leaderBoid.position.z);
    ImGui::Text("Bando (Alvo): %.1f %.1f %.1f", flockCenter.x, flockCenter.y, flockCenter.z);
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
    
    leaderBoid.position = glm::vec3(0, 15, 30.0f); // Posição inicial afastada da torre

    flock.clear();
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