#include "SnowEngine.h"
#include <random>

// 构造函数
SnowEngine::SnowEngine() {}

// 析构函数
SnowEngine::~SnowEngine() { m_snowflakes.clear(); }

float SnowEngine::RandomFloat(float min, float max)
{
    static std::random_device             rd;
    static std::mt19937                   gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

// 辅助函数：重置雪花状态
void SnowEngine::ResetSnowflake(Snowflake &s, int screenWidth, int screenHeight)
{
    s.landed = false;
    s.life   = 1.0f;  // 满血复活
    s.x      = RandomFloat(0.0f, (float)screenWidth);

    // 决定大小
    s.maxSize = RandomFloat(2.0f, 5.0f);
    s.size    = s.maxSize;
    s.speed   = RandomFloat(1.0f, 3.0f);
    s.angle   = RandomFloat(0.0f, 3.14f * 2);

    // --- 核心修改：重生时必须在屏幕上方 ---
    // 设为负数，保证它从屏幕外落下，这样就不会一出生就卡在顶部窗口上
    s.y = RandomFloat(-50.0f, -5.0f);
}

void SnowEngine::Initialize(int screenWidth, int screenHeight)
{
    m_snowflakes.clear();
    int count = 3000;  // 雪花数量

    for (int i = 0; i < count; i++)
    {
        Snowflake s;
        s.x      = RandomFloat(0.0f, (float)screenWidth);
        s.y      = RandomFloat(-(float)screenHeight, -5.0f);
        s.speed  = RandomFloat(1.0f, 2.0f);
        s.size   = RandomFloat(3.0f, 6.0f);
        s.angle  = RandomFloat(0.0f, 3.14f * 2);  // 随机初始角度
        s.landed = false;
        m_snowflakes.push_back(s);
    }
}

void SnowEngine::Update(int                          screenWidth,
                        int                          screenHeight,
                        const std::vector<Obstacle> &obstacles)
{
    for (auto &s : m_snowflakes)
    {
        // ================= Case A: 堆积/融化状态
        // (这一部分您之前改得没问题，保持逻辑) =================
        if (s.landed)
        {
            bool isStillSafe = false;

            // Z-Order 扫描：检查我现在脚下踩的地方，是不是被别人盖住了？
            // 或者那个地方是不是变成了“不可积雪”的状态？
            for (const auto &obs : obstacles)
            {
                if (s.x >= obs.rect.left && s.x <= obs.rect.right &&
                    s.y >= obs.rect.top && s.y <= obs.rect.bottom)
                {
                    // 命中了某个窗口 (Z-Order 从上到下)
                    if (abs(s.y - obs.rect.top) < 10.0f)
                    {
                        // 我在它的表面。
                        // 只有当它允许积雪时，我才安全。
                        // 如果它是最大化窗口
                        // (canAccumulate=false)，那我就站不住了。
                        if (obs.canAccumulate)
                        {
                            isStillSafe = true;
                        }
                        else
                        {
                            isStillSafe = false;  // 光滑表面，滑落
                        }
                        break;  // 找到了最近的接触面，不用看后面了
                    }
                    else
                    {
                        // 我在它的内部 -> 说明我被盖住了 -> 不安全
                        isStillSafe = false;
                        break;
                    }
                }
            }

            if (!isStillSafe)
            {
                s.landed = false;  // 恢复下落
                // 稍微往下推一点，防止下一帧立刻判定碰撞造成闪烁
                s.y += 2.0f;
                continue;
            }

            // 融化逻辑
            float meltSpeed = 0.005f;
            s.life -= meltSpeed;
            s.size = s.maxSize * s.life;

            if (s.life <= 0.0f)
            {
                // 彻底融化后，回天上重生
                ResetSnowflake(s, screenWidth, screenHeight);
            }
            continue;
        }

        // ================= Case B: 空中飘落状态 (这里是 Bug 的源头！)
        // =================

        s.y += s.speed;
        s.angle += 0.03f;
        s.x += sin(s.angle) * 0.5f;
        s.x += 0.2f;  // 风力

        // 碰撞检测
        if (s.y > 0 && s.y < screenHeight)
        {
            // 必须使用索引遍历，因为我们需要回溯前面的窗口 (j < i)
            for (size_t i = 0; i < obstacles.size(); ++i)
            {
                const auto &obs = obstacles[i];

                // 1. 物理接触检测
                if (s.x >= obs.rect.left && s.x <= obs.rect.right)
                {
                    // 预测下一帧会不会撞上顶部
                    if (s.y >= obs.rect.top &&
                        s.y <= obs.rect.top + s.speed + 5.0f)
                    {
                        // --- 修复点 1：检查属性 ---
                        // 如果这个障碍物被标记为“不可积雪” (比如最大化窗口)
                        // 那就假装没看见，继续往下掉
                        if (!obs.canAccumulate)
                        {
                            continue;
                        }

                        // --- 修复点 2：局部遮挡检测 (Raycast) ---
                        // 我虽然撞到了 obs[i]，但我头顶上有人吗？
                        bool isOccluded = false;
                        for (size_t j = 0; j < i; ++j)
                        {
                            const auto &higherObs = obstacles[j];
                            // 检查点 (s.x, s.y) 是否在更高层窗口的矩形内
                            if (s.x >= higherObs.rect.left &&
                                s.x <= higherObs.rect.right &&
                                s.y >= higherObs.rect.top &&
                                s.y <= higherObs.rect.bottom)
                            {
                                isOccluded = true;
                                break;
                            }
                        }

                        if (isOccluded)
                        {
                            // 被挡住了，这次碰撞无效，继续掉
                            continue;
                        }

                        // 一切正常，着陆！
                        s.y      = (float)obs.rect.top;
                        s.landed = true;
                        s.life   = 1.0f;
                        break;  // 停止检测其他障碍物
                    }
                }
            }
        }

        // 边界检查
        if (s.y > screenHeight)
        {
            ResetSnowflake(s, screenWidth, screenHeight);
        }
    }
}

void SnowEngine::Render(ID2D1HwndRenderTarget    *pRenderTarget,
                        ID2D1RadialGradientBrush *pBrush)
{
    pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (const auto &s : m_snowflakes)
    {
        // 只有看得见的才画
        if (s.size > 0.1f)
        {
            // 动态调整透明度
            float opacity = 0.8f;

            // 如果正在融化，透明度也跟着降低
            if (s.landed)
            {
                opacity *= s.life;
            }

            pBrush->SetOpacity(opacity);

            // 每次都需要设置变换吗？其实可以优化，
            // 但为了简单，直接修改 Center 和 Radius
            pBrush->SetCenter(D2D1::Point2F(s.x, s.y));
            pBrush->SetRadiusX(s.size);
            pBrush->SetRadiusY(s.size);

            pRenderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(s.x, s.y), s.size, s.size), pBrush);
        }
    }
}