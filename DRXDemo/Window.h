#pragma once

#include <string>
#include <functional>
#include <stdexcept>
#include <set>
#include "utilities.h"
#include "framework.h"
#include "Resource.h"

namespace DRXDemo
{
    class Window
    {
    public:
        using OnPaintCallback = std::function<void(void)>;
        using OnKeyboardCallback = std::function<void(uint8_t)>;

        Window(HINSTANCE hInstance, std::wstring title, uint32_t width, uint32_t height);

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

        inline void SetInitialized(bool initialized)
        {
            _initialized = initialized;
        }

        inline void ShowWindow(bool showWindow)
        {
            ::ShowWindow(_hWnd, showWindow);
            ::UpdateWindow(_hWnd);
        }

        inline void Quit()
        {
            return ::PostQuitMessage(0);
        }

        // Subscribe methods
        inline void SubscribeOnPaint(OnPaintCallback& callback)
        {
            _onPaintSubscribers.insert(&callback);
        }
        
        inline void SubscribeOnKeyDown(OnKeyboardCallback& callback)
        {
            _onKeyDownSubscribers.insert(&callback);
        }
        
        inline void SubscribeOnKeyUp(OnKeyboardCallback& callback)
        {
            _onKeyUpSubscribers.insert(&callback);
        }

        // Unsubscribe methods
        inline void UnsubscribeOnPaint(OnPaintCallback& callback)
        {
            _onPaintSubscribers.erase(&callback);
        }

        inline void UnsubscribeOnKeyDown(OnKeyboardCallback& callback)
        {
            _onKeyDownSubscribers.erase(&callback);
        }

        inline void UnsubscribeOnKeyUp(OnKeyboardCallback& callback)
        {
            _onKeyUpSubscribers.erase(&callback);
        }

        int32_t Run();

    private:

        inline static bool _windowClassRegistered = false;

        HINSTANCE _hInstance;
        HWND _hWnd;
        std::wstring _title;
        uint32_t _width;
        uint32_t _height;
        bool _initialized;
        std::set<OnPaintCallback*> _onPaintSubscribers;
        std::set<OnKeyboardCallback*> _onKeyDownSubscribers;
        std::set<OnKeyboardCallback*> _onKeyUpSubscribers;

        inline static const std::wstring _windowClass = L"WindowClass";

        void _OnPaint();
        void _OnKeyDown(uint8_t key);
        void _OnKeyUp(uint8_t key);

        ATOM _RegisterWindowClass();
        HWND _CreateWindow();
        LRESULT CALLBACK _WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK _StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    };
};