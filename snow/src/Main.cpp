// Main.cpp : 定义应用程序的入口点。

#include "framework.h"
#include "snow.h"
#include "SnowEngine.h"
#include "WindowUtils.h"

#include <vector>
#include <d2d1.h>
#include <dwmapi.h>
#include <shellapi.h>  // --- 露露叶新增：托盘图标必须的头文件 ---

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")
// shell32.lib 通常是默认链接的，如果报错请加上 #pragma comment(lib,
// "shell32.lib")

// --- 托盘图标相关的定义 ---
#define WM_TRAYICON      (WM_USER + 1)  // 自定义消息 ID
#define ID_TRAY_ICON     1001           // 图标 ID
#define IDM_TRAY_EXIT    1002           // 菜单：退出
#define IDM_TRAY_SETTING 1003           // 菜单：设置
#define IDT_TIMER_SNOW   1004           // 雪花刷新定时器

// --- Direct2D 全局变量 ---
ID2D1Factory                *pD2DFactory    = nullptr;
ID2D1HwndRenderTarget       *pRenderTarget  = nullptr;
ID2D1RadialGradientBrush    *pRadialBrush   = nullptr;
ID2D1GradientStopCollection *pGradientStops = nullptr;

// --- 定义全局引擎实例 ---
SnowEngine g_Engine;

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE             hInst;
WCHAR                 szTitle[MAX_LOADSTRING];
WCHAR                 szWindowClass[MAX_LOADSTRING];
std::vector<Obstacle> g_Obstacles;
NOTIFYICONDATA g_nid = {};  // --- 露露叶新增：托盘图标数据结构 ---

// 前向声明:
ATOM             MyRegisterClass(HINSTANCE hInstance);
BOOL             InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// --- 露露叶新增：托盘图标管理函数 ---
void InitNotifyIcon(HWND hWnd)
{
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd   = hWnd;
    g_nid.uID    = ID_TRAY_ICON;
    // NIF_MESSAGE: 允许发送消息, NIF_ICON: 显示图标, NIF_TIP: 显示提示文字
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage =
        WM_TRAYICON;  // 当用户点击图标时，发送这个消息给 WndProc

    // 加载图标：优先尝试加载应用图标，如果没有就用系统默认图标
    g_nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SNOW));
    if (!g_nid.hIcon)
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    // 鼠标悬停时的提示文字
    wcscpy_s(g_nid.szTip, L"Desktop Snow (右键菜单)");

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void DeleteNotifyIcon() { Shell_NotifyIcon(NIM_DELETE, &g_nid); }

// 渲染函数 (保持不变)
void Render(HWND hWnd)
{
    if (!pRenderTarget)
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                     &pD2DFactory)))
            return;

        D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
            DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED);

        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT, pixelFormat);

        if (FAILED(pD2DFactory->CreateHwndRenderTarget(
                props,
                D2D1::HwndRenderTargetProperties(hWnd, size),
                &pRenderTarget)))
            return;

        // 强制 DPI 为 96 (1:1 物理像素)
        pRenderTarget->SetDpi(96.0f, 96.0f);

        if (!pRadialBrush)
        {
            D2D1_GRADIENT_STOP stops[] = {
                {0.0f, D2D1::ColorF(D2D1::ColorF::White, 1.0f)},
                {1.0f, D2D1::ColorF(D2D1::ColorF::White, 0.0f)}};

            if (FAILED(pRenderTarget->CreateGradientStopCollection(
                    stops,
                    2,
                    D2D1_GAMMA_2_2,
                    D2D1_EXTEND_MODE_CLAMP,
                    &pGradientStops)))
                return;

            if (FAILED(pRenderTarget->CreateRadialGradientBrush(
                    D2D1::RadialGradientBrushProperties(
                        D2D1::Point2F(0, 0), D2D1::Point2F(0, 0), 0, 0),
                    pGradientStops,
                    &pRadialBrush)))
                return;
        }
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    g_Engine.Render(pRenderTarget, pRadialBrush);
    pRenderTarget->EndDraw();
}

int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR        lpCmdLine,
                      _In_ int           nCmdShow)
{
    // DPI 感知 (必须第一行)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    // 强制使用硬编码的类名，确保和 WindowUtils 匹配
    wcscpy_s(szWindowClass, MAX_LOADSTRING, L"SnowWindowClass");

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // 主循环前的数据初始化
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 【瀑布式开场】预热障碍物数据
    // 注意：这里 g_Obstacles 已经在 InitInstance 里被填充过一次了
    // 所以 g_Engine 初始化时能读到正确的数据
    g_Engine.Initialize(screenW, screenH);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SNOW));
    MSG    msg;

    // --- 露露叶修改：回归最纯朴的消息循环 ---
    // 不再需要 PeekMessage 和那些复杂的 else 逻辑了
    // 所有的驱动力都来自 WM_TIMER 消息
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 资源清理
    if (pRadialBrush)
        pRadialBrush->Release();
    if (pGradientStops)
        pGradientStops->Release();
    if (pRenderTarget)
        pRenderTarget->Release();
    if (pD2DFactory)
        pD2DFactory->Release();

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNOW));
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    wcex.hbrBackground = nullptr;

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND hWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT |
                                    WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                szWindowClass,
                                szTitle,
                                WS_POPUP | WS_VISIBLE,
                                screenX,
                                screenY,
                                screenW,
                                screenH,
                                nullptr,
                                nullptr,
                                hInstance,
                                nullptr);

    if (!hWnd)
        return FALSE;

    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    UpdateWindow(hWnd);

    // --- 预热障碍物 ---
    g_Obstacles = WindowUtils::GetObstacles(hWnd);

    // --- 露露叶新增：创建托盘图标 ---
    InitNotifyIcon(hWnd);
    // --- 露露叶新增：启动心脏起搏器 ---
    // 每 15 毫秒触发一次 WM_TIMER (大约 66 FPS)
    SetTimer(hWnd, IDT_TIMER_SNOW, 15, nullptr);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static ULONGLONG lastObstacleUpdate = 0;

    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hWnd);
        break;

        // --- 露露叶新增：心脏跳动逻辑 ---
    case WM_TIMER:
        if (wParam == IDT_TIMER_SNOW)
        {
            // 1. 定时更新障碍物 (每 500ms)
            ULONGLONG tick = GetTickCount64();
            if (tick - lastObstacleUpdate > 500)
            {
                g_Obstacles        = WindowUtils::GetObstacles(hWnd);
                lastObstacleUpdate = tick;
            }

            // 2. 更新物理引擎
            int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            g_Engine.Update(sw, sh, g_Obstacles);

            // 3. 渲染
            // 注意：Render 可能会被阻塞，如果电脑卡顿，WM_TIMER 可能会积压，
            // 但对于这种小工具来说，直接在这里 Render 是最简单的方案。
            Render(hWnd);
        }
        break;

    // --- 露露叶新增：处理托盘消息 ---
    case WM_TRAYICON:
        // lParam 包含了具体的鼠标事件 (如 WM_RBUTTONUP, WM_LBUTTONDBLCLK)
        if (lParam == WM_RBUTTONUP)
        {
            // 获取鼠标位置
            POINT pt;
            GetCursorPos(&pt);

            // 创建弹出菜单 (Popup Menu)
            HMENU hMenu = CreatePopupMenu();

            // 添加菜单项 (这里我们手动添加，不用资源文件，更灵活)
            // 参数: 菜单句柄, 标志位, 命令ID, 显示的文本
            AppendMenu(hMenu, MF_STRING, IDM_TRAY_SETTING, L"设置 (Settings)");
            AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);  // 分隔线
            AppendMenu(hMenu, MF_STRING, IDM_TRAY_EXIT, L"退出 (Exit)");

            // 这是一个 Win32 的怪癖：弹出菜单前必须把窗口设为前台，
            // 否则点击菜单外的地方，菜单不会自动消失。
            SetForegroundWindow(hWnd);

            // 在鼠标位置显示菜单
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

            // 用完销毁
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        // 处理托盘菜单的点击
        case IDM_TRAY_EXIT:
            DestroyWindow(hWnd);  // 触发 WM_DESTROY
            break;
        case IDM_TRAY_SETTING:
            MessageBox(hWnd,
                       L"设置功能即将在下一个版本上线！\n请等待女仆酱的更新~",
                       L"提示",
                       MB_OK | MB_ICONINFORMATION);
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC         hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        KillTimer(hWnd, IDT_TIMER_SNOW);  // 关掉定时器
        // 记得在窗口销毁时删除图标，不然它会变成僵尸图标留在任务栏
        DeleteNotifyIcon();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}