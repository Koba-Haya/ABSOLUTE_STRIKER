#pragma once
#include "Vector.h"
#include <cstdint>

// HLSL 側の GPUParticle(GPUParticle.hlsli)と 1:1 でレイアウトを合わせること(StructuredBuffer なので、4 バイト単位でそのまま並ぶ)
struct GPUParticle {
    Vector3 translate;
    Vector3 scale;
    float lifeTime;
    Vector3 velocity;
    float currentTime;
    Vector4 color;
    // 色の変化と、フェードの形(スタイル付きのエミッタが決める。スタイルなしでは、色は変わらず、フェードは線形)
    Vector3 colorStart;  // 生まれたときの色
    Vector3 colorEnd;    // 冷めきったときの色(寿命の coolRate 分の1 の時間で、colorStart から変わる)
    float coolRate;      // 色が変わる速さ(1 / 色が変わりきる寿命の割合)
    float fadePower;     // アルファ = 1 - (経過割合 ^ fadePower)。1 で線形、大きいほど終わり際まで残ってから消える
    // 発光パス(雲のあとに加算で描く層)用
    float heat;          // 熱さ(1 = 出たて、0 = 冷めきった灰)。Update が、色の変化(cool)から求める
    float glowOnly;      // 1 なら、通常の描画では描かず、発光パスだけで描く(核の光など)
    float glowGain;      // 発光パスの明るさの倍率
};

// 粒を描くときの深度の扱い
enum class GPUParticleDepth : uint32_t {
    Test = 0,         // 深度テストあり・書き込みなし(通常。物体の後ろに隠れる)
    TestAndWrite = 1, // 深度テストあり・書き込みあり(雲の合成の前に描く粒用。雲の合成は深度を見るので、粒の位置までの雲だけが粒にかかる)
    None = 2,         // 深度なし(雲の合成のあと、描画先に深度が無いとき)
};

// エミッタの形状（Sphere: 球内ランダム配置 / Box: 直方体内ランダム配置）
enum class GPUEmitterShape : uint32_t {
    Sphere = 0,
    Box = 1,
};

// 1エミッタ分のGPU側データ。HLSL側のEmitter(GPUParticle.hlsli)と1:1でレイアウトを合わせること。
// 16バイト境界をまたがないよう、144バイト(16バイトのブロック9つ)ちょうどになるよう手動でパディングしている。
struct GPUEmitter {
    Vector3 translate;   // offset 0
    float radius;         // offset 12  (0-15: 16B)
    Vector3 halfExtents; // offset 16
    float glowOnly;       // offset 28  (16-31: 16B)  スタイル付き: 1 なら発光パスだけで描く
    uint32_t shape;        // offset 32
    uint32_t count;         // offset 36
    float frequency;       // offset 40
    float frequencyTime;   // offset 44 (32-47: 16B)
    uint32_t emit;          // offset 48
    uint32_t enabled;       // offset 52
    float coolRatio;        // offset 56 (スタイル付き: 色が変わりきる寿命の割合)
    float fadePower;        // offset 60 (48-63: 16B)

    // --- スタイル(styled が 0 のときは使わない。従来どおり、色・大きさ・速さ・寿命をランダムに決める) ---
    Vector3 colorStart;   // offset 64
    float lifeMin;         // offset 76 (64-79: 16B)
    Vector3 colorEnd;     // offset 80
    float lifeMax;         // offset 92 (80-95: 16B)
    float sizeMin;         // offset 96
    float sizeMax;         // offset 100
    float speedMin;        // offset 104
    float speedMax;        // offset 108 (96-111: 16B)
    Vector3 baseVelocity; // offset 112 (全ての粒に足す速度。向きの偏りや、発生源の速度の引き継ぎに使う)
    uint32_t styled;       // offset 124 (112-127: 16B)
    float glowGain;        // offset 128 (発光パスの明るさの倍率)
    float pad4[3];         // offset 132 (128-143: 16B)
};

static constexpr uint32_t kMaxGPUEmitters = 8;

// b0 にバインドするConstantBuffer本体。8要素×144バイト=1152バイトで16Bアライン済み。
struct GPUEmitterArray {
    GPUEmitter emitters[kMaxGPUEmitters];
};

// GPU Particleに適用するフィールド（CG3で学んだFieldのGPU Particle版）
enum class GPUFieldType : uint32_t {
    None = 0,
    Attractor = 1,  // targetへ吸い寄せる
    Wind = 2,       // directionへ一定方向の力を加える(重力も、これで表す)
    Vortex = 3,     // direction軸を中心に渦を巻かせる
    Drag = 4,       // 速度に比例して減速させる(strength: 1 秒あたりの減衰率)
    Turbulence = 5, // 位置と時間で変わる、ゆらぎの力(strength: 力の大きさ、direction.x: 空間の細かさ、direction.y: 変化の速さ)
    Spring = 6,     // targetへ、距離に比例した力で引き寄せる(strength: ばね定数。遠い粒ほど強く引かれ、近づくと弱まる。Drag と組むと収束する)
};

// 1Field分のデータ。こちらも32バイト(16Bブロック2つ)でレイアウトを固定する。
struct GPUField {
    Vector3 target;     // offset 0
    float strength;      // offset 12 (0-15: 16B)
    Vector3 direction;  // offset 16
    uint32_t type;        // offset 28 (16-31: 16B)
};

static constexpr uint32_t kMaxGPUFields = 5;

struct GPUFieldArray {
    GPUField fields[kMaxGPUFields]; // 5*32=160バイト
    uint32_t count;                  // offset 160
    float pad[3];                    // offset 164 (160-175: 16B)
};

// 発光パス(雲の合成のあとに、加算で描く層)の調整値。HLSL の GPUParticleGlow.hlsli の GlowParams と同じ並び(16 バイトのブロックごと)
struct GPUGlowParams {
    float streakLength = 0.35f;     // 尾の長さ: 速さ(画面上、ユニット / 秒)1 あたりの伸び(粒の大きさに対する倍率)
    float streakMax = 6.0f;         // 尾の長さの上限(粒の大きさに対する倍率)
    float twinkleAmount = 0.35f;    // ちらつきの深さ(0 〜 1)
    float twinkleSpeed = 9.0f;      // ちらつきの速さ(ラジアン / 秒。粒ごとに位相がずれる)
    float intensity = 1.0f;         // 全体の明るさ
    float coreSharpness = 40.0f;    // 光の芯の鋭さ(大きいほど小さく鋭い)
    float haloSharpness = 9.0f;     // 光輪の鋭さ
    float outerSharpness = 2.6f;    // 外光(大きなやわらかい光)の鋭さ
    Vector3 hotCenter{ 0.0f, 0.0f, 0.0f }; // 熱を帯びる中心(巣)
    float hotRadius = 6.0f;         // 中心からこの距離までの粒が、熱を帯びる
    float hotBoost = 0.0f;          // 中心に近い粒の熱さに足す量(集まりが進むほど上げる)
    float speedHeatGain = 0.25f;    // 速い粒ほど熱くする量(速さ 1 あたり)
    float depthBias = 0.0005f;      // 深度テストの余裕(粒が島にめり込んで見えないように)
    float time = 0.0f;              // ちらつきの時刻(秒)
    Vector3 colorRed{ 0.55f, 0.06f, 0.02f };    // 熱さの色の傾き: 低温(暗い赤)
    float distanceFalloff = 0.012f; // カメラから遠いほど暗くする量(距離 1 あたりの指数的な減衰率)
    Vector3 colorOrange{ 1.00f, 0.42f, 0.08f }; // 中温(橙)
    float pad1 = 0.0f;
    Vector3 colorYellow{ 1.00f, 0.78f, 0.30f }; // 高温(黄)
    float pad2 = 0.0f;
    Vector3 colorCore{ 1.00f, 0.96f, 0.80f };   // 光の芯(白に近い黄)
    float pad3 = 0.0f;
};
