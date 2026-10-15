#include "pokergame.h"

int main() {
    const int windowWidth = 1200;
    const int windowHeight = 800;

    InitWindow(windowWidth, windowHeight, "Texas Hold'em Poker");
    SetTargetFPS(60); 

    PokerGame game;

    while (!WindowShouldClose()) {
        
        game.update();

        game.draw();
    }

    CloseWindow();

    return 0;
} 
