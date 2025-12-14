#include "SnowEngine.h"
#include <random>

// 构造函数
SnowEngine::SnowEngine() {}

// 析构函数
SnowEngine::~SnowEngine()
{
    m_snowflakes.clear();
    DiscardDeviceResources();  // 记得析构时清理图片
}

// 释放显存资源
void SnowEngine::DiscardDeviceResources()
{
    if (m_pSnowBitmap)
    {
        m_pSnowBitmap->Release();
        m_pSnowBitmap = nullptr;
    }
}

// 均匀分布采样
float SnowEngine::RandomFloat(float min, float max)
{
    static std::random_device             rd;
    static std::mt19937                   gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

// 正态分布采样
float SnowEngine::RandomNormal(float mean, float stddev)
{
    static std::random_device rd;
    static std::mt19937       gen(rd());

    // 使用 std::normal_distribution 生成符合高斯分布的随机数
    std::normal_distribution<float> dis(mean, stddev);

    return dis(gen);
}

// 辅助函数：重置雪花状态
void SnowEngine::ResetSnowflake(Snowflake &s, int screenWidth, int screenHeight)
{
    s.landed = false;
    s.life   = 1.0f;  // 满血复活

    // 随机出生在屏幕上方
    s.x = RandomFloat(0.0f, (float)screenWidth);
    s.y = RandomFloat(-50.0f, -10.0f);

    // === 核心修改：大小使用正态分布 ===
    // 均值 5.0 (大部分雪花是中等偏大)
    // 标准差 2.0 (允许一定的波动)
    float rawSize = RandomNormal(5.0f, 2.0f);

    // [重要] 截断 (Clamp)
    // 即使是正态分布，也要防止出现太离谱的值
    if (rawSize < 2.5f)
        rawSize = 2.5f;  // 最小限制
    if (rawSize > 12.0f)
        rawSize = 12.0f;  // 最大限制 (偶尔出现的特大雪花)

    s.maxSize = rawSize;
    s.size    = s.maxSize;

    // === 速度与大小挂钩 (模拟景深) ===
    // 基础速度 + 大小加成 (越大的落得越快)
    float baseSpeed = 1.0f + (s.size - 2.5f) * 0.4f;

    // 速度也加一点点正态扰动，让它更自然
    s.speed = baseSpeed + RandomNormal(0.0f, 0.2f);

    // 防止速度过慢倒着飞
    if (s.speed < 0.5f)
        s.speed = 0.5f;

    // === 初始相位 ===
    s.angle = RandomFloat(0.0f, 6.28f);
}

void SnowEngine::Initialize(int screenWidth, int screenHeight)
{
    m_snowflakes.clear();
    int count = 500;  // 雪花数量

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
        // ================= Case A: 堆积/融化状态 =================
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

        // ================= Case B: 空中飘落状态 =================

        // [摇摆相位]
        // 既然要随机性，那摇摆的频率(变化快慢)也可以和大小挂钩
        // 小雪花飘得急(频率高)，大雪花飘得缓(频率低)
        float frequency = 0.02f + (10.0f - s.size) * 0.005f;
        s.angle += frequency;

        // [摇摆幅度]
        // sin(s.angle) 产生 -1 ~ 1 的波形
        // 0.5f 是基础摆动幅度
        float swing = sin(s.angle) * 0.5f;

        // === 优化 3: 差异化风力 ===
        // 我们利用 s.speed (它已经包含了大小信息) 作为系数
        // 速度快(大/近)的雪花，横向移动也应该快一点 (视差)
        // 0.5f 是一个调节系数，你可以改
        float effectiveWind = m_windForce * (s.speed * 0.5f);

        // 应用位置更新
        s.x += effectiveWind + swing;    // 差异化风力 + 独立摇摆
        s.y += s.speed * m_speedFactor;  // 差异化速度 * 全局重力倍率

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

// 创建“印章”
void SnowEngine::CreateSnowBitmap(ID2D1HwndRenderTarget *pRenderTarget)
{
    // 1. 创建一个临时的“画布”，大小为 32x32
    // 我们画一个高清晰度的雪花，然后渲染时缩放它，这样效果最好
    ID2D1BitmapRenderTarget *pCompatibleRenderTarget = nullptr;
    D2D1_SIZE_F              size = D2D1::SizeF(32.0f, 32.0f);

    HRESULT hr = pRenderTarget->CreateCompatibleRenderTarget(
        size, &pCompatibleRenderTarget);
    if (FAILED(hr))
        return;

    // 2. 在临时画布上画一个完美的渐变雪花
    pCompatibleRenderTarget->BeginDraw();
    pCompatibleRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));  // 透明背景

    // 创建临时的渐变刷子
    ID2D1RadialGradientBrush    *pTempBrush = nullptr;
    ID2D1GradientStopCollection *pTempStops = nullptr;
    D2D1_GRADIENT_STOP           stops[]    = {
        {0.0f, D2D1::ColorF(D2D1::ColorF::White, 1.0f)},  // 中心白
        {1.0f, D2D1::ColorF(D2D1::ColorF::White, 0.0f)}  // 边缘透
    };

    pCompatibleRenderTarget->CreateGradientStopCollection(
        stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pTempStops);

    if (pTempStops)
    {
        pCompatibleRenderTarget->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(
                D2D1::Point2F(16, 16), D2D1::Point2F(0, 0), 16, 16),
            pTempStops,
            &pTempBrush);
    }

    if (pTempBrush)
    {
        pCompatibleRenderTarget->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(16, 16), 16, 16), pTempBrush);
    }

    pCompatibleRenderTarget->EndDraw();

    // 3. 把画好的结果取出来，存成位图 (印章)
    pCompatibleRenderTarget->GetBitmap(&m_pSnowBitmap);

    // 4. 清理临时工具
    if (pTempBrush)
        pTempBrush->Release();
    if (pTempStops)
        pTempStops->Release();
    pCompatibleRenderTarget->Release();
}

void SnowEngine::Render(ID2D1HwndRenderTarget    *pRenderTarget,
                        ID2D1RadialGradientBrush *pBrush)
{
    // 如果“印章”还没做，赶紧做一个
    if (!m_pSnowBitmap)
    {
        CreateSnowBitmap(pRenderTarget);
        if (!m_pSnowBitmap)
            return;  // 创建失败就别画了
    }

    pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (const auto &s : m_snowflakes)
    {
        if (s.size > 0.1f)
        {
            // 动态调整透明度
            float opacity = 0.8f;
            if (s.landed)
                opacity *= s.life;

            // --- 核心差异：从 FillEllipse 变成了 DrawBitmap ---

            // 计算目标矩形：把 32x32 的印章，缩放到 s.size 大小
            // s.x, s.y 是中心点
            D2D1_RECT_F destRect = D2D1::RectF(
                s.x - s.size, s.y - s.size, s.x + s.size, s.y + s.size);

            // 盖章！
            pRenderTarget->DrawBitmap(m_pSnowBitmap,
                                      destRect,
                                      opacity,
                                      D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                      NULL  // 源矩形 NULL 表示使用整个位图
            );
        }
    }
}

// 调整雪花数量
void SnowEngine::SetFlakeCount(int count)
{
    // 限制一下范围，别把电脑炸了
    if (count < 0)
        count = 0;
    if (count > 5000)
        count = 5000;

    int currentSize = (int)m_snowflakes.size();
    if (count > currentSize)
    {
        // 需要增加：补足差额
        for (int i = 0; i < count - currentSize; ++i)
        {
            Snowflake s;
            // 随便给个位置，之后 Reset 会修正
            s.x      = 0;
            s.y      = -10.0f;
            s.landed = false;
            s.life   = 0.0f;  // 设为0让它重生
            m_snowflakes.push_back(s);
        }
    }
    else if (count < currentSize)
    {
        // 需要减少：直接截断
        m_snowflakes.resize(count);
    }
}

// 调整雪花重力（下降速度）
void SnowEngine::SetGravity(float g) { m_speedFactor = g; }

// 调整雪花风力（左右飘动）
void SnowEngine::SetWind(float w) { m_windForce = w; }
