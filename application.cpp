#include "include/application.h"

//#include "glad.c"

//#ifdef _WIN32

//#include <GL/wglext.h>
//#include <windows.h>

//#define EXAMPLE_NAME "genmap"

//typedef HGLRC(WINAPI *PFNGLXCREATECONTEXTATTRIBS)(HDC hDC, HGLRC hShareContext, const int *attribList);

//class Win32Application
//{
//public:
//    bool Startup(
//        std::function<bool()> intialize,
//        std::function<void(int width, int height)> resize,
//        std::function<void()> destroy);

//    int Run(
//        std::function<bool(double time)> tick);

//private:
//    std::function<void(int width, int height)> _resize;
//    std::function<void()> _destroy;
//    HINSTANCE _hInstance;
//    HWND _hWnd;
//    HDC _hDC;
//    HGLRC _hRC;
//    PFNGLXCREATECONTEXTATTRIBS _pfnGlxCreateContext;
//    InputState _inputState;

//    virtual void Destroy(
//        const char *errorMessage = nullptr);

//    LRESULT CALLBACK objectProc(
//        UINT uMsg,
//        WPARAM wParam,
//        LPARAM lParam);

//    static LRESULT CALLBACK staticProc(
//        HWND hWnd,
//        UINT uMsg,
//        WPARAM wParam,
//        LPARAM lParam);
//};

//bool Win32Application::Startup(
//    std::function<bool()> intialize,
//    std::function<void(int width, int height)> resize,
//    std::function<void()> destroy)
//{
//    _resize = resize;
//    _destroy = destroy;
//    _hInstance = GetModuleHandle(nullptr);

//    WNDCLASS wc;

//    if (GetClassInfo(_hInstance, EXAMPLE_NAME, &wc) == FALSE)
//    {
//        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
//        wc.lpfnWndProc = (WNDPROC)Win32Application::staticProc;
//        wc.cbClsExtra = 0;
//        wc.cbWndExtra = 0;
//        wc.hInstance = _hInstance;
//        wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
//        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
//        wc.hbrBackground = nullptr;
//        wc.lpszMenuName = nullptr;
//        wc.lpszClassName = EXAMPLE_NAME;

//        if (RegisterClass(&wc) == FALSE)
//        {
//            Destroy("Failed to register window class");
//            return false;
//        }
//    }

//    _hWnd = CreateWindowEx(
//        WS_EX_APPWINDOW,
//        EXAMPLE_NAME, EXAMPLE_NAME,
//        WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
//        CW_USEDEFAULT, CW_USEDEFAULT,
//        CW_USEDEFAULT, CW_USEDEFAULT,
//        nullptr, nullptr,
//        _hInstance, (VOID *)this);

//    if (_hWnd == 0)
//    {
//        Destroy("Failed to create window");
//        return false;
//    }

//    _hDC = GetDC(_hWnd);

//    if (_hDC == 0)
//    {
//        Destroy("Failed to get device context");
//        return false;
//    }

//    static PIXELFORMATDESCRIPTOR pfd =
//        {
//            sizeof(PIXELFORMATDESCRIPTOR), // Size Of This Pixel Format Descriptor
//            1,                             // Version Number
//            PFD_DRAW_TO_WINDOW |           // Format Must Support Window
//                PFD_SUPPORT_OPENGL |       // Format Must Support CodeGL
//                PFD_DOUBLEBUFFER,          // Must Support Double Buffering
//            PFD_TYPE_RGBA,                 // Request An RGBA Format
//            32,                            // Select Our Color Depth
//            0, 0, 0, 0, 0, 0,              // Color Bits Ignored
//            0,                             // No Alpha Buffer
//            0,                             // Shift Bit Ignored
//            0,                             // No Accumulation Buffer
//            0, 0, 0, 0,                    // Accumulation Bits Ignored
//            16,                            // 16Bit Z-Buffer (Depth Buffer)
//            0,                             // No Stencil Buffer
//            0,                             // No Auxiliary Buffer
//            PFD_MAIN_PLANE,                // Main Drawing Layer
//            0,                             // Reserved
//            0, 0, 0                        // Layer Masks Ignored
//        };

//    auto pixelFormat = ChoosePixelFormat(_hDC, &pfd);
//    if (pixelFormat == false)
//    {
//        Destroy("Failed to choose pixel format");
//        return false;
//    }

//    if (SetPixelFormat(_hDC, pixelFormat, &pfd) == FALSE)
//    {
//        Destroy("Failed to set pixel format");
//        return false;
//    }

//    _pfnGlxCreateContext = (PFNGLXCREATECONTEXTATTRIBS)wglGetProcAddress("wglCreateContextAttribsARB");
//    if (_pfnGlxCreateContext == nullptr)
//    {
//        _hRC = wglCreateContext(_hDC);

//        if (_hRC == 0)
//        {
//            Destroy("Failed to create clasic opengl context (v1.0)");
//            return false;
//        }
//    }
//    else
//    {
//        GLint attribList[] =
//            {
//                WGL_CONTEXT_MAJOR_VERSION_ARB,
//                3,
//                WGL_CONTEXT_MINOR_VERSION_ARB,
//                3,
//                WGL_CONTEXT_FLAGS_ARB,
//                0,
//                0,
//            };

//        _hRC = _pfnGlxCreateContext(_hDC, 0, attribList);
//        if (_hRC == 0)
//        {
//            Destroy("Failed to create modern opengl context (v3.3)");
//            return false;
//        }
//    }

//    wglMakeCurrent(_hDC, _hRC);

//    gladLoadGL();

//    spdlog::debug("GL_VERSION                  : {0}", glGetString(GL_VERSION));
//    spdlog::debug("GL_SHADING_LANGUAGE_VERSION : {0}", glGetString(GL_SHADING_LANGUAGE_VERSION));
//    spdlog::debug("GL_RENDERER                 : {0}", glGetString(GL_RENDERER));
//    spdlog::debug("GL_VENDOR                   : {0}", glGetString(GL_VENDOR));

//    if (!intialize())
//    {
//        Destroy("Initialize failed");
//        return false;
//    }

//    ShowWindow(_hWnd, SW_SHOW);
//    SetForegroundWindow(_hWnd);
//    SetFocus(_hWnd);

//    return true;
//}

//int Win32Application::Run(
//    std::function<bool(double time)> tick)
//{
//    bool running = true;

//    while (running)
//    {
//        MSG msg;
//        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
//        {
//            if (msg.message == WM_QUIT)
//            {
//                break;
//            }
//            else
//            {
//                TranslateMessage(&msg);
//                DispatchMessage(&msg);
//            }
//        }

//        if (IsWindowVisible(_hWnd) == FALSE)
//        {
//            break;
//        }

//        running = tick(0);

//        SwapBuffers(_hDC);
//    }

//    _destroy();

//    return 0;
//}

//void Win32Application::Destroy(
//    const char *errorMessage)
//{
//    if (errorMessage != nullptr)
//    {
//        spdlog::error(errorMessage);
//    }

//    _destroy();

//    UnregisterClass(EXAMPLE_NAME, _hInstance);

//    if (_hDC != 0)
//    {
//        ReleaseDC(_hWnd, _hDC);
//        _hDC = 0;
//    }

//    if (_hWnd != 0)
//    {
//        DestroyWindow(_hWnd);
//        _hWnd = 0;
//    }
//}

//LRESULT CALLBACK Win32Application::objectProc(
//    UINT uMsg,
//    WPARAM wParam,
//    LPARAM lParam)
//{
//    switch (uMsg)
//    {
//        case WM_SIZE:
//        {
//            auto width = LOWORD(lParam);
//            auto height = HIWORD(lParam);

//            this->_resize(width, height);

//            break;
//        }
//    }
//    return DefWindowProc(this->_hWnd, uMsg, wParam, lParam);
//}

//LRESULT CALLBACK Win32Application::staticProc(
//    HWND hWnd,
//    UINT uMsg,
//    WPARAM wParam,
//    LPARAM lParam)
//{
//    Win32Application *app = nullptr;

//    if (uMsg == WM_NCCREATE)
//    {
//        app = reinterpret_cast<Win32Application *>(((LPCREATESTRUCT)lParam)->lpCreateParams);

//        if (app != nullptr)
//        {
//            app->_hWnd = hWnd;

//            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<long long>(app));

//            return app->objectProc(uMsg, wParam, lParam);
//        }
//    }
//    else
//    {
//        app = reinterpret_cast<Win32Application *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

//        if (app != nullptr)
//        {
//            return app->objectProc(uMsg, wParam, lParam);
//        }
//    }

//    return DefWindowProc(hWnd, uMsg, wParam, lParam);
//}

//Win32Application *CreateApplication(
//    std::function<bool()> initialize,
//    std::function<void(int width, int height)> resize,
//    std::function<void()> destroy)
//{
//    static Win32Application app;

//    if (app.Startup(initialize, resize, destroy))
//    {
//        return &app;
//    }

//    spdlog::error("Create application failed");

//    exit(0);

//    return nullptr;
//}
//#endif // _WIN32

//#ifdef __linux__
//#endif // __linux__

//template <class TApp>
//int Application::CreateAndRun(TApp &t)
//{
//    auto app = CreateApplication(
//        [&]() { return t.Startup(); },
//        [&](int w, int h) { t.Resize(w, h); },
//        [&]() { t.Destroy(); });

//    return app->Run([&]() { return t.Tick(); });
//}
