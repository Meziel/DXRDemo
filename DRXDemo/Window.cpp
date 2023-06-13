#pragma once

#include "Window.h"

namespace DRXDemo
{
    Window::Window(HINSTANCE hInstance, std::wstring title, uint32_t width, uint32_t height) :
        _hInstance(hInstance),
        _title(title),
        _width(width),
        _height(height)
    {
        // Register window class if haven't already
        if (!_windowClassRegistered)
        {
            _RegisterWindowClass();
            _windowClassRegistered = true;
        }

        // Create window
        _hWnd = _CreateWindow();

        if (!_hWnd)
        {
            std::string errorMessage = GetLastErrorAsString();
            OutputDebugStringA(errorMessage.c_str());
            throw std::runtime_error(errorMessage);
        }
    }

    int32_t Window::Run()
    {
        HACCEL hAccelTable = LoadAccelerators(_hInstance, MAKEINTRESOURCE(IDC_DRXDEMO));
        MSG msg;

        while (GetMessage(&msg, nullptr, 0, 0))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return static_cast<int32_t>(msg.wParam);
    }

    void Window::_OnPaint()
    {
        for (auto subscriber : _onPaintSubscribers)
        {
            subscriber->operator()();
        }
    }

    void Window::_OnKeyDown(uint8_t key)
    {
        for (auto subscriber : _onKeyDownSubscribers)
        {
            subscriber->operator()(key);
        }
    }

    void Window::_OnKeyUp(uint8_t key)
    {
        for (auto subscriber : _onKeyUpSubscribers)
        {
            subscriber->operator()(key);
        }
    }

    inline ATOM Window::_RegisterWindowClass()
    {
        WNDCLASSEXW wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = _StaticWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = _hInstance;
        wcex.hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_DRXDEMO));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DRXDEMO);
        wcex.lpszClassName = _windowClass.c_str();
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

        return RegisterClassExW(&wcex);
    }

    HWND Window::_CreateWindow()
    {
        int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

        RECT windowRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };
        ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
        int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
        int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

        HWND hWnd = CreateWindowW(_windowClass.c_str(), _title.c_str(), WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, nullptr, nullptr, _hInstance, nullptr);

        // Embed window object in hwnd
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        return hWnd;
    }

    LRESULT CALLBACK Window::_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        // Don't handle events until DirextX initialized
        if (_initialized)
        {
            switch (message)
            {
            case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                // Parse the menu selections:
                switch (wmId)
                {
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
                }
            }
            break;
            case WM_PAINT:
                _OnPaint();
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            case WM_KEYDOWN:
                _OnKeyDown(static_cast<uint8_t>(wParam));
                break;
            case WM_KEYUP:
                _OnKeyUp(static_cast<uint8_t>(wParam));
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        else
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        return 0;
    }

    LRESULT CALLBACK Window::_StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Window* windowInst = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (windowInst)
        {
            return windowInst->_WndProc(hWnd, message, wParam, lParam);
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
};