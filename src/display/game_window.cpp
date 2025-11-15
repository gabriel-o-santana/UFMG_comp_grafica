#include "display/game_window.hpp"
#include "shaders/shader.hpp"
#include <iostream>
#include <vector>
#include <cmath>

// Bibliotecas de Matemática (GLM)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- Variáveis Globais para o Mundo ---
Shader s;

// Chão
unsigned int VAO_Floor, VBO_Floor;

// Torre (Cone)
unsigned int VAO_Cone, VBO_Cone;
int coneVertexCount = 0;

// Configurações da Tela (usadas para a câmera)
const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

// Função chamada quando a janela é redimensionada
void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Função Auxiliar para criar a geometria do Cone
void CreateCone(float radius, float height, int segments) {
    std::vector<float> vertices;
    
    // Ponto do topo (pico)
    float topX = 0.0f;
    float topY = height;
    float topZ = 0.0f;

    // Centro da base
    float baseX = 0.0f;
    float baseY = 0.0f;
    float baseZ = 0.0f;

    for (int i = 0; i < segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * 3.1415926f;
        float nextAngle = (float)(i + 1) / (float)segments * 2.0f * 3.1415926f;

        // Coordenadas da borda da base
        float x1 = radius * cos(angle);
        float z1 = radius * sin(angle);
        float x2 = radius * cos(nextAngle);
        float z2 = radius * sin(nextAngle);

        // Triângulo da lateral (Pico -> Base1 -> Base2)
        vertices.push_back(topX); vertices.push_back(topY); vertices.push_back(topZ);
        vertices.push_back(x1);   vertices.push_back(baseY); vertices.push_back(z1);
        vertices.push_back(x2);   vertices.push_back(baseY); vertices.push_back(z2);

        // Triângulo da base (Centro -> Base2 -> Base1) - inverte a ordem para a normal apontar para baixo
        vertices.push_back(baseX); vertices.push_back(baseY); vertices.push_back(baseZ);
        vertices.push_back(x2);    vertices.push_back(baseY); vertices.push_back(z2);
        vertices.push_back(x1);    vertices.push_back(baseY); vertices.push_back(z1);
    }

    coneVertexCount = vertices.size() / 3; // 3 floats por vértice (x, y, z)

    // Configuração OpenGL do Cone
    glGenVertexArrays(1, &VAO_Cone);
    glGenBuffers(1, &VBO_Cone);
    glBindVertexArray(VAO_Cone);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Cone);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Atributo 0: Posição (3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// 1. Inicialização da Janela
void GameWindow::Initialize() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
}

// 2. Carregamento de Conteúdo (Geometria e Shaders)
void GameWindow::LoadContent() {
    glfwSetFramebufferSizeCallback(this->windowHandle, FramebufferSizeCallback);

    // Inicializa ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(this->windowHandle, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    std::cout << "INFO::IMGUI::SUCCESSFULLY_INITIALIZED" << std::endl;

    // Carrega os Shaders
    s = Shader::LoadShader("resources/shaders/testing.vs", "resources/shaders/testing.fs");

    // --- CONFIGURAÇÃO DO CHÃO ---
    // Um quadrado grande no plano Y=0
    float floorVertices[] = {
         50.0f, 0.0f,  50.0f, 
        -50.0f, 0.0f,  50.0f, 
        -50.0f, 0.0f, -50.0f, 
        
         50.0f, 0.0f,  50.0f, 
        -50.0f, 0.0f, -50.0f, 
         50.0f, 0.0f, -50.0f  
    };

    glGenVertexArrays(1, &VAO_Floor);
    glGenBuffers(1, &VBO_Floor);
    glBindVertexArray(VAO_Floor);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Floor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // --- CONFIGURAÇÃO DA TORRE ---
    // Cria o cone proceduralmente
    CreateCone(2.0f, 5.0f, 32);

    // Habilita teste de profundidade (Z-Buffer) para o 3D funcionar corretamente
    glEnable(GL_DEPTH_TEST);
}

void GameWindow::Update() {
    // Recarrega shader se o arquivo for alterado
    s.ReloadFromFile();
}

void GameWindow::Render() {
    // Limpa a tela e o buffer de profundidade
    glClearColor(0.5f, 0.8f, 0.9f, 1.0f); // Cor do céu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Inicia o frame da interface (ImGui)
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Ativa o Shader
    s.use();

    // --- CÂMERA (View & Projection) ---
    // Projection: Perspectiva
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    s.setMat4("projection", projection);

    // View: Posiciona a câmera no alto olhando para o centro
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 15.0f, 25.0f), // Eye
                                 glm::vec3(0.0f, 0.0f, 0.0f),   // Center
                                 glm::vec3(0.0f, 1.0f, 0.0f));  // Up
    s.setMat4("view", view);

    // --- DESENHO DO CHÃO ---
    glm::mat4 model = glm::mat4(1.0f); // Identidade (sem transformação)
    s.setMat4("model", model);
    s.setVec3("objectColor", 0.2f, 0.5f, 0.2f); // Verde
    
    glBindVertexArray(VAO_Floor);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- DESENHO DA TORRE ---
    model = glm::mat4(1.0f); // Torre na origem (0,0,0)
    s.setMat4("model", model);
    s.setVec3("objectColor", 0.7f, 0.7f, 0.7f); // Cinza
    
    glBindVertexArray(VAO_Cone);
    glDrawArrays(GL_TRIANGLES, 0, coneVertexCount);

    // Renderiza a interface por cima de tudo
    // (Adicione seus controles aqui depois)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Troca os buffers
    glfwSwapBuffers(this->windowHandle);
    glfwPollEvents();
}

void GameWindow::Unload() {
    // Limpeza
    glDeleteVertexArrays(1, &VAO_Floor);
    glDeleteBuffers(1, &VBO_Floor);
    glDeleteVertexArrays(1, &VAO_Cone);
    glDeleteBuffers(1, &VBO_Cone);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}