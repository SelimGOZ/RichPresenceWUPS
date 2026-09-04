#include <thread>
#include <cstring>
#include <string>
#include <fmt/core.h>

#if defined(_WIN32)
    #include "win.hpp"
    #include <windows.h>
    #include <shellapi.h>
    #define WM_TRAYICON (WM_USER + 1)
    #define ID_TRAY_QUIT 1001
    NOTIFYICONDATAW nid = {};
#elif defined(__linux__) || defined(__APPLE__)
    #include "unix.hpp"
#endif

std::string repo = "flamingnineteen/richpresencewups-db";
bool DEBUG_LOGS = false;

void initializeApp(int argc, char* argv[]) {
    int i = 2;
    while (i < argc) {
        if (std::strcmp(argv[i - 1], "repo") == 0) {
            repo = argv[i];
            fmt::println("Using repository {}.", repo);
        }
        else if (std::strcmp(argv[i - 1], "port") == 0) {
            UDP_PORT = std::stoi(argv[i]);
            fmt::println("Using port {}.", UDP_PORT);
        }
        else if (std::strcmp(argv[i - 1], "debug") == 0) {
            DEBUG_LOGS = (std::string(argv[i]) == "true");
        }
        i += 2;
    }
}

#if defined(_WIN32)
void SetupConsole() {
    if (DEBUG_LOGS) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::printf("Debug console allocated.\n");
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT cursor;
                GetCursorPos(&cursor);
                SetForegroundWindow(hwnd);

                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_QUIT, L"Quit Wii U Rich Presence");

                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, cursor.x, cursor.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_QUIT) {
                Shell_NotifyIconW(NIM_DELETE, &nid);

                discord::RPCManager::get().shutdown();

                std::exit(0);
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
#endif

void RunCoreLogic(int argc, char* argv[]) {
    initializeApp(argc, argv);

    std::thread tthread(checkIdle);

    discordSetup();
    discord::RPCManager::get().initialize();

    gameLoop(repo);

    runIdleLoop = false;
    if (tthread.joinable()) {
        tthread.join();
    }

    discord::RPCManager::get().shutdown();
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    int argc = __argc;
    char** argv = __argv;

    initializeApp(argc, argv);
    SetupConsole();

    const wchar_t* CLASS_NAME = L"WiiURPCtrayClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"WiiU RPC Tray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    wcscpy_s(nid.szTip, L"Wii U Rich Presence");
    Shell_NotifyIconW(NIM_ADD, &nid);

    std::thread worker([argc, argv]() {
        RunCoreLogic(argc, argv);
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
    });

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (worker.joinable()) {
        worker.join();
    }

    return 0;
}
#else
int main(int argc, char* argv[]) {
    RunCoreLogic(argc, argv);
    return 0;
}
#endif
