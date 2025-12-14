#pragma once
#include <vector>
#include <d2d1.h>
#include "WindowUtils.h"

// 1. 雪花依然是简单的 struct (数据)
struct Snowflake
{
    float x;
    float y;
    float speed;
    float size;
    float angle;    // 后面做摇摆用
    bool  landed;   // 是否着陆
    float life;     // 堆积后的寿命 (1.0 -> 0.0)
    float maxSize;  // 记住它原本的大小，用于融化时缩放
};

// 2. 引擎类 (逻辑)
class SnowEngine
{
  public:
    SnowEngine();
    ~SnowEngine();

    // 初始化：传入屏幕大小
    void Initialize(int screenWidth, int screenHeight);

    // 更新：传入屏幕大小（应对分辨率改变）
    void Update(int                          screenWidth,
                int                          screenHeight,
                const std::vector<Obstacle> &obstacles);

    // 渲染：传入 Direct2D 的 RenderTarget 和画笔
    void Render(ID2D1HwndRenderTarget    *pRenderTarget,
                ID2D1RadialGradientBrush *pBrush);

  private:
    std::vector<Snowflake> m_snowflakes;  // 这里管理所有雪花

    // 辅助函数
    float RandomFloat(float min, float max);

    // 重置单颗雪花的函数(复用逻辑)
    void ResetSnowflake(Snowflake &s, int screenWidth, int screenHeight);
};
