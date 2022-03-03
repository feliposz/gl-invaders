#include <windows.h> 
#include <GL/gl.h> 
#include "invaders.h"

// globals and defines
HWND  ghWnd;
HDC   ghDC;
HGLRC ghRC;
HINSTANCE ghInstance;
int gnCmdShow;

LONG WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);

// platform services to game code

const LPCSTR ClassName = "Invaders!";
const LPCSTR WindowName = "Invaders!";

bool create_window(int width, int height)
{
    WNDCLASSA   wndclass;

    /* Register the frame class */
    wndclass.style = 0;
    wndclass.lpfnWndProc = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = ghInstance;
    wndclass.hIcon = NULL;
    wndclass.hCursor = NULL;
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndclass.lpszMenuName = WindowName;
    wndclass.lpszClassName = ClassName;

    if (!RegisterClassA(&wndclass))
        return false;

    /* Create the frame */
    ghWnd = CreateWindowA(ClassName,
                          WindowName,
                          WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          width,
                          height,
                          NULL,
                          NULL,
                          ghInstance,
                          NULL);

    /* make sure window was created */
    if (!ghWnd)
        return false;

    /* show and update main window */
    ShowWindow(ghWnd, gnCmdShow);

    UpdateWindow(ghWnd);

    return true;
}

void window_clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void swap_buffers()
{
    SwapBuffers(ghDC);
}

bool update_window_events()
{
    MSG        msg;

    /*
    *  Process all pending messages
    */

    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == TRUE)
    {
        if (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            return true;
        }
    }
        
    return false;
}

void do_sleep(int ms)
{
    Sleep(ms);
}

LARGE_INTEGER gPerfFrequency;

double get_time()
{
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return (double)ticks.QuadPart / (double)gPerfFrequency.QuadPart;
}

int event_buffer_first = 0;
int event_buffer_last = 0;
const int event_buffer_max = 1000;
Event events[event_buffer_max];

bool get_next_event(Event *event)
{
    if (event_buffer_first != event_buffer_last)
    {
        *event = events[event_buffer_first++];
        if (event_buffer_first == event_buffer_max)
        {
            event_buffer_first = 0;
        }
        return true;
    }
    return false;
}

Event *add_event()
{
    Event *event = &events[event_buffer_last++];
    event->type = EVENT_TYPE_NONE;
    event->key_code = KEY_NONE;
    event->key_pressed = true;
    if (event_buffer_last == event_buffer_max)
    {
        event_buffer_last = 0;
    }
    if (event_buffer_last == event_buffer_first)
    {
        event = NULL;
    }
    return event;
}

BOOL bSetupPixelFormat(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd, *ppfd;
    int pixelformat;

    ppfd = &pfd;

    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    ppfd->nVersion = 1;
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    ppfd->dwLayerMask = PFD_MAIN_PLANE;
    ppfd->iPixelType = PFD_TYPE_COLORINDEX;
    ppfd->cColorBits = 8;
    ppfd->cDepthBits = 16;
    ppfd->cAccumBits = 0;
    ppfd->cStencilBits = 0;

    pixelformat = ChoosePixelFormat(hdc, ppfd);

    if ((pixelformat = ChoosePixelFormat(hdc, ppfd)) == 0)
    {
        MessageBox(NULL, TEXT("ChoosePixelFormat failed"), TEXT("Error"), MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hdc, pixelformat, ppfd) == FALSE)
    {
        MessageBox(NULL, TEXT("SetPixelFormat failed"), TEXT("Error"), MB_OK);
        return FALSE;
    }

    return TRUE;
}

// main window procedure
LONG WINAPI MainWndProc(
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 1;
    PAINTSTRUCT    ps;
    RECT rect;

    switch (uMsg)
    {

        case WM_CREATE:
            ghDC = GetDC(hWnd);
            if (!bSetupPixelFormat(ghDC))
                PostQuitMessage(0);

            ghRC = wglCreateContext(ghDC);
            wglMakeCurrent(ghDC, ghRC);
            GetClientRect(hWnd, &rect);
            //initializeGL(rect.right, rect.bottom);
            break;

        case WM_CLOSE:
        {
            if (ghRC)
                wglDeleteContext(ghRC);
            if (ghDC)
                ReleaseDC(hWnd, ghDC);
            ghRC = 0;
            ghDC = 0;

            DestroyWindow(hWnd);
            Event *event = add_event();
            event->type = EVENT_TYPE_QUIT;
        } break;

        case WM_DESTROY:
            if (ghRC)
                wglDeleteContext(ghRC);
            if (ghDC)
                ReleaseDC(hWnd, ghDC);

            PostQuitMessage(0);
            break;

        case WM_KEYDOWN:
        {
            Event *event = add_event();
            event->type = EVENT_TYPE_KEYBOARD;
            event->key_pressed = true;

            switch (wParam)
            {
                case VK_LEFT:
                    event->key_code = KEY_ARROW_LEFT;
                    break;
                case VK_RIGHT:
                    event->key_code = KEY_ARROW_RIGHT;
                    break;
                case VK_UP:
                    event->key_code = KEY_ARROW_UP;
                    break;
                case VK_DOWN:
                    event->key_code = KEY_ARROW_DOWN;
                    break;
                case VK_SHIFT:
                    event->key_code = KEY_SHIFT;
                    break;
                case VK_ESCAPE:
                    event->key_code = KEY_ESCAPE;
                    break;
            }

        } break;

        case WM_KEYUP:
        {
            Event * event = add_event();
            event->type = EVENT_TYPE_KEYBOARD;
            event->key_pressed = false;

            switch (wParam)
            {
                case VK_LEFT:
                    event->key_code = KEY_ARROW_LEFT;
                    break;
                case VK_RIGHT:
                    event->key_code = KEY_ARROW_RIGHT;
                    break;
                case VK_UP:
                    event->key_code = KEY_ARROW_UP;
                    break;
                case VK_DOWN:
                    event->key_code = KEY_ARROW_DOWN;
                    break;
                case VK_SHIFT:
                    event->key_code = KEY_SHIFT;
                    break;
                case VK_ESCAPE:
                    event->key_code = KEY_ESCAPE;
                    break;
            }

        } break;

        default:
            lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
            break;
    }

    return lRet;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    QueryPerformanceFrequency(&gPerfFrequency);
    ghInstance = hInstance;
    gnCmdShow = nCmdShow;
    invaders();
}
