#include "fado.h"

internal void HandleGameInput(FGameState* gameState, FGameInput* input)
{
    FGameControllerInput* controllerInput = &input->controller;
    if (!controllerInput->isConnected)
    {
        return;
    }

    if (controllerInput->back.isDown)
    {
        gameState->running = false;
    }

}

void GameUpdate(FEngineMemory* memory, FGameState* gameState, FGameInput* input)
{
	HandleGameInput(gameState, input);
}

/*
* Quick debug messasge
#include <stdio.h>
#include <windows.h>
    for (i32 i = 0; i < ArrayCount(controllerInput->buttons); ++i)
    {
        if (controllerInput->buttons[i].isDown)
        {
            char logBuffer[256];
            sprintf_s(logBuffer, "Button %i is down, Held Time: %f\n", i, controllerInput->buttons[i].heldLength);
            OutputDebugStringA(logBuffer);
        }
    }
*/