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

    // 资源清理：当设备丢失或重置时，需要清理缓存的位图
    void DiscardDeviceResources();

    // --- 新增参数控制 ---
    void SetFlakeCount(int count);
    void SetGravity(float gravity);
    void SetWind(float wind);

  private:
    std::vector<Snowflake> m_snowflakes;  // 这里管理所有雪花

    float m_speedFactor = 1.0f;  // 默认 1.0
    float m_windForce   = 0.0f;  // 默认 0.0

    // 缓存的雪花位图
    ID2D1Bitmap *m_pSnowBitmap = nullptr;

    // 均匀分布采样
    float RandomFloat(float min, float max);

    // 正态分布采样
    float RandomNormal(float mean, float stddev);

    // 重置单颗雪花的函数(复用逻辑)
    void ResetSnowflake(Snowflake &s, int screenWidth, int screenHeight);

    // 内部函数：创建母版图片
    void CreateSnowBitmap(ID2D1HwndRenderTarget *pRenderTarget);
};
