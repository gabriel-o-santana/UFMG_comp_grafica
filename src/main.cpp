#include "display/game_window.hpp"

int main() {

GameWindow gw = GameWindow{ 800, 600, "Simulação de Boids – Trabalho Prático" };
    return gw.Run();
}