#include "app.h"
#include "scene.h"

#include <stdio.h>
#include <stdlib.h>

#define WINDOW_NAME     "Learn Software Rasterization"

#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600

#define MIN_FRAME_TIME (1.0 / 60.0)

struct {
    HDC         hdc;
    HWND        hwnd;
    HINSTANCE   instance;
    app*        app;
    int         active;
    int         exit;
} state;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        state.hdc = GetDC(hwnd);

        RECT rect;
        ZeroMemory(&rect, sizeof(RECT));
        if (GetWindowRect(hwnd, &rect)) {
            if (app_create_surface(state.app,
                state.hdc, rect.right - rect.left, rect.bottom - rect.top) != LSRERR_OK) {
                MessageBoxA(NULL, "Unable to create render surface.", "Error", MB_ICONERROR | MB_OK);
            }
        }
        return 0;
    } break;
    case WM_DESTROY: {
        state.exit = TRUE;
        ReleaseDC(state.hwnd, state.hdc);
        PostQuitMessage(EXIT_SUCCESS);
        return 0;
    }  break;
    case WM_SIZE: {
        if (app_resize_surface(state.app, LOWORD(lp), HIWORD(lp)) != LSRERR_OK) {
            MessageBoxA(NULL, "Unable to resize render surface.", "Error", MB_ICONERROR | MB_OK);
        }
        return 0;
    } break;
    case WM_ACTIVATE: {
        state.active = LOWORD(wp) != WA_INACTIVE;
        return 0;
    } break;
    case WM_SETFOCUS: {
        state.active = TRUE;
        return 0;
    } break;
    case WM_KILLFOCUS: {
        state.active = FALSE;
        return 0;
    } break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        ZeroMemory(&ps, sizeof(PAINTSTRUCT));
        HDC hdc = BeginPaint(state.hwnd, &ps);
        app_blt_surface(state.app, &ps.rcPaint, hdc, NULL);
        EndPaint(state.hwnd, &ps);
        ReleaseDC(state.hwnd, hdc);
        return 0;
    } break;
    case WM_KEYDOWN: {
        if (wp == VK_ESCAPE) {
            state.exit = TRUE;
        }
        app_key_down(state.app, (int)wp);
        return 0;
    } break;
    case WM_KEYUP: {
        app_key_up(state.app, (int)wp);
        return 0;
    } break;
    case WM_MOUSEMOVE: {
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        app_mouse_move(state.app, &point);
    } break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        if (msg == WM_RBUTTONDOWN) {
            SetCapture(hwnd);
        }
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        app_mouse_down(state.app, &point,
            msg == WM_LBUTTONDOWN ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT);
    } break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        if (msg == WM_RBUTTONUP) {
            ReleaseCapture();
        }
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        app_mouse_up(state.app, &point,
            msg == WM_LBUTTONUP ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT);
    } break;
    case WM_ACTIVATEAPP: {
        state.active = wp != FALSE;
        return 0;
    } break;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND create_window() {
    WNDCLASSEXA cls;
    ZeroMemory(&cls, sizeof(WNDCLASSEXA));

    cls.cbSize = sizeof(WNDCLASSEXA);
    cls.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = window_proc;
    cls.hInstance = state.instance;
    cls.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    cls.lpszClassName = WINDOW_NAME;

    if (RegisterClassExA(&cls) == NULL) {
        return NULL;
    }

    return CreateWindowExA(WS_EX_OVERLAPPEDWINDOW, WINDOW_NAME, WINDOW_NAME,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, state.instance, NULL);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: lsr <.obj file path>\n");
        return EXIT_FAILURE;
    }

    int result = LSRERR_OK;
    if ((result = app_create(&state.app)) != LSRERR_OK) {
        MessageBoxA(NULL, "Unable to create application.", "Error", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    if ((result = app_load_scene(state.app, argv[1])) != LSRERR_OK) {
        MessageBoxA(NULL, "Unable to load scene.", "Error", MB_ICONERROR | MB_OK);
        app_release(state.app);
        return EXIT_FAILURE;
    }

    // Window
    state.instance = GetModuleHandleA(NULL);
    state.hwnd = create_window();
    if (state.hwnd == NULL) {
        MessageBoxA(NULL, "Unable to create window.", "Error", MB_ICONERROR | MB_OK);
        app_release(state.app);
        return EXIT_FAILURE;
    }

    // Show window
    ShowWindow(state.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(state.hwnd);

    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    end.QuadPart = start.QuadPart;

    while (!state.exit) {
        while (PeekMessageA(&msg, state.hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                state.exit = TRUE;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (state.exit) {
            break;
        }

        if (!state.active) {
            continue;
        }

        const f64 elapsed = (f64)(start.QuadPart - end.QuadPart) / freq.QuadPart;

        if ((result = app_execute(state.app, elapsed)) != LSRERR_OK) {
            MessageBoxA(NULL, "A fatal error occurred.", "Error", MB_ICONERROR | MB_OK);
            app_release(state.app);
            return EXIT_FAILURE;
        }

        QueryPerformanceCounter(&end);

        {
            const f64 length = (f64)(end.QuadPart - start.QuadPart) / freq.QuadPart;
            if (length < MIN_FRAME_TIME) {
                Sleep((DWORD)(1000.0 * (MIN_FRAME_TIME - length)));
            }
        }

        end.QuadPart = start.QuadPart;
        QueryPerformanceCounter(&start);
        InvalidateRect(state.hwnd, NULL, FALSE);
    }

    UnregisterClassA(WINDOW_NAME, state.instance);

    app_release(state.app);

    return EXIT_SUCCESS;
}
