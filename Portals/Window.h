#pragma once

#include <string>
#include <functional>
#include <stdexcept>
#include <set>
#include "utilities.h"
#include "framework.h"
#include "Resource.h"

namespace Portals
{
    class Window
    {
    public:
        using OnPaintCallback = std::function<void(void)>;

        inline Window(HINSTANCE hInstance, std::wstring title, uint32_t width, uint32_t height) :
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

        inline HWND GetHWND() const
        {
            return _hWnd;
        }

        inline const std::wstring& GetTitle() const
        {
            return _title;
        }

        inline uint32_t GetWidth() const
        {
            return _width;
        }

        inline uint32_t GetHeight() const
        {
            return _height;
        }

        void SetInitialized(bool initialized)
        {
            _initialized = initialized;
        }

        void ShowWindow(bool showWindow)
        {
            ::ShowWindow(_hWnd, showWindow);
            ::UpdateWindow(_hWnd);
        }

        int32_t Run()
        {
            HACCEL hAccelTable = LoadAccelerators(_hInstance, MAKEINTRESOURCE(IDC_PORTALS));
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

        inline void SubscribeOnPaint(OnPaintCallback& callback)
        {
            _onPaintSubscribers.insert(&callback);
        }

        inline void UnsubscribeOnPaint(OnPaintCallback& callback)
        {
            _onPaintSubscribers.erase(&callback);
        }

    private:

        inline static bool _windowClassRegistered = false;

        HINSTANCE _hInstance;
        HWND _hWnd;
        std::wstring _title;
        uint32_t _width;
        uint32_t _height;
        bool _initialized;
        std::set<std::function<void(void)>*> _onPaintSubscribers;

        inline static const std::wstring _windowClass = L"WindowClass";

        void _OnPaint()
        {
            for (auto subscriber : _onPaintSubscribers)
            {
                subscriber->operator()();
            }
        }

        inline ATOM _RegisterWindowClass()
        {
            WNDCLASSEXW wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = _StaticWndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = _hInstance;
            wcex.hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_PORTALS));
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PORTALS);
            wcex.lpszClassName = _windowClass.c_str();
            wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

            return RegisterClassExW(&wcex);
        }

        inline HWND _CreateWindow()
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

        inline LRESULT CALLBACK _WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
                {
                case 'V':
                    //g_VSync = !g_VSync;
                    break;
                case VK_ESCAPE:
                    ::PostQuitMessage(0);
                    break;
                }
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

        inline static LRESULT CALLBACK _StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            Window* windowInst = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            if (windowInst)
            {
                return windowInst->_WndProc(hWnd, message, wParam, lParam);
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    };
};