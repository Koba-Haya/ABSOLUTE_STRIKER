// 発光パス(雲の合成のあとに、加算で描く層)の調整値。
// GPUParticleManager::GlowParams(GPUParticleManager.h)と、16 バイトのブロックごとに 1:1 で並びを合わせること。
struct GlowParams {
    float32_t streakLength;   // 尾の長さ(速さ 1 あたりの伸び)
    float32_t streakMax;      // 尾の長さの上限
    float32_t twinkleAmount;  // ちらつきの深さ
    float32_t twinkleSpeed;   // ちらつきの速さ

    float32_t intensity;      // 全体の明るさ
    float32_t coreSharpness;  // 光の芯の鋭さ
    float32_t haloSharpness;  // 光輪の鋭さ
    float32_t outerSharpness; // 外光の鋭さ

    float32_t3 hotCenter;     // 熱を帯びる中心(巣)
    float32_t hotRadius;

    float32_t hotBoost;       // 中心に近い粒の熱さに足す量
    float32_t speedHeatGain;  // 速い粒ほど熱くする量
    float32_t depthBias;      // 深度テストの余裕
    float32_t time;           // ちらつきの時刻

    float32_t3 colorRed;      // 熱さの色の傾き: 低温
    float32_t distanceFalloff; // カメラから遠いほど暗くする量

    float32_t3 colorOrange;   // 中温
    float32_t pad1;

    float32_t3 colorYellow;   // 高温
    float32_t pad2;

    float32_t3 colorCore;     // 光の芯
    float32_t pad3;
};
