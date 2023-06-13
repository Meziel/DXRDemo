#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>

#include "framework.h"

#include "Window.h"
#include "Game.h"
#include "DXContext.h"

using namespace DRXDemo;

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

    Window window(hInstance, L"DRX Demo", width, height);
    DXContext dxContext(window, 3);

    if (!dxContext.IsRaytracingSupported())
    {
        assert("Raytracing not supported on device");
        return EXIT_FAILURE;
    }

    Game game(window, dxContext, width, height);

    // TODO: move to game
    Window::OnPaintCallback onPaintCallback = [&game]() {
        game.Update();
        game.Render();
    };

    Window::OnKeyboardCallback onKeyboardDownCallback = [&game](uint8_t key) {
        game.OnKeyDown(key);
    };

    Window::OnKeyboardCallback onKeyboardUpCallback = [&game](uint8_t key) {
        game.OnKeyUp(key);
    };

    window.SubscribeOnPaint(onPaintCallback);
    window.SubscribeOnKeyDown(onKeyboardDownCallback);
    window.SubscribeOnKeyUp(onKeyboardUpCallback);

    window.SetInitialized(true);
    window.ShowWindow(true);
    window.Run();

    // Cleanup
    window.UnsubscribeOnPaint(onPaintCallback);
    window.UnsubscribeOnKeyDown(onKeyboardDownCallback);
    window.UnsubscribeOnKeyUp(onKeyboardUpCallback);

    return EXIT_SUCCESS;
}