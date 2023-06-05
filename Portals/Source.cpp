// Portals.cpp : Defines the entry point for the application.
//

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>

#include "framework.h"

#include "Window.h"
#include "Game.h"
#include "DXContext.h"

using namespace Portals;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const uint32_t width = 1280;
    const uint32_t height = 720;

    Window window(hInstance, L"Portals", width, height);
    DXContext dxContext(window, 3);
    Game game(dxContext, width, height);

    Window::OnPaintCallback onPaintCallback = [&game]() {
        game.Update();
        game.Render();
    };

    window.SubscribeOnPaint(onPaintCallback);

    window.SetInitialized(true);
    window.ShowWindow(true);
    window.Run();

    // Cleanup
    window.UnsubscribeOnPaint(onPaintCallback);

    return EXIT_SUCCESS;
}