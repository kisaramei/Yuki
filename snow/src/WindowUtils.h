#pragma once
#include <windows.h>
#include <vector>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// --- 修改 1：升级结构体，加入标记 ---
struct Obstacle
{
    RECT rect;
    bool canAccumulate;  // true=正常积雪, false=我是墙但不积雪(如最大化窗口)
};

struct SearchContext
{
    std::vector<Obstacle> *pResult;
    std::vector<RECT>      blockers;
};

class WindowUtils
{
  public:
    static std::vector<Obstacle> GetObstacles(HWND myHwnd)
    {
        std::vector<Obstacle> obstacles;
        std::vector<RECT>     blockers;

        // 1. 获取任务栏
        HWND hTaskBar = FindWindow(L"Shell_TrayWnd", NULL);
        if (hTaskBar && IsWindowVisible(hTaskBar))
        {
            RECT rc;
            GetWindowRect(hTaskBar, &rc);
            // 任务栏既是障碍物(可积雪)，也是遮挡物
            obstacles.push_back({rc, true});
            blockers.push_back(rc);
        }

        // 2. 遍历窗口
        SearchContext ctx;
        ctx.pResult  = &obstacles;
        ctx.blockers = blockers;

        EnumWindows(EnumWindowsProc, (LPARAM)&ctx);

        return obstacles;
    }

  private:
    static bool IsFullyCovered(const RECT &target, const RECT &blocker)
    {
        // 容差修正
        const long TOLERANCE = 20;
        return target.left >= (blocker.left - TOLERANCE) &&
               target.right <= (blocker.right + TOLERANCE) &&
               target.top >= (blocker.top - TOLERANCE) &&
               target.bottom <= (blocker.bottom + TOLERANCE);
    }

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
    {
        auto *pCtx = (SearchContext *)lParam;

        if (!IsWindowVisible(hwnd))
            return TRUE;
        if (IsIconic(hwnd))
            return TRUE;

        WCHAR className[256];
        GetClassName(hwnd, className, 256);
        if (wcscmp(className, L"SnowWindowClass") == 0)
            return TRUE;
        if (wcscmp(className, L"Progman") == 0)
            return TRUE;
        if (wcscmp(className, L"WorkerW") == 0)
            return TRUE;

        RECT    rcFrame;
        HRESULT hr = DwmGetWindowAttribute(
            hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rcFrame, sizeof(rcFrame));
        if (FAILED(hr))
            GetWindowRect(hwnd, &rcFrame);

        // [Step 1] 遮挡检查
        for (const auto &blocker : pCtx->blockers)
        {
            if (IsFullyCovered(rcFrame, blocker))
                return TRUE;  // 被完全挡住，剔除
        }

        // [Step 2] 注册为遮挡物 (我是墙！)
        pCtx->blockers.push_back(rcFrame);

        // [Step 3] 积雪属性判定
        bool isSlippery = (rcFrame.top < 10);  // 贴顶窗口(最大化)是光滑的

        if (isSlippery)
        {
            // 虽然是墙，但不能积雪
            pCtx->pResult->push_back({rcFrame, false});
        }
        else
        {
            // 正常窗口，可以积雪
            pCtx->pResult->push_back({rcFrame, true});
        }

        return TRUE;
    }
};