#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include "GPUParticle.h"
#include "Matrix.h"
#include "BlendMode.h" // ブレンドモード列挙体
#include <vector>

class DirectXCommon;
class TextureResource;

class GPUParticleManager {
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    // 発光パスの調整値(GPUParticle.h)
    using GlowParams = GPUGlowParams;

    static GPUParticleManager* GetInstance();

    void Initialize(DirectXCommon* dx);
    void Update();
    // 粒の動きの時間の速さ(1 で通常、0 で止まる)。ヒットストップなど、演出で粒だけをゆっくりにするときに使う。
    // 常駐エミッタの射出の間隔には掛けない。演出が終わったら 1 に戻すこと
    void SetTimeScale(float scale) { timeScale_ = scale; }
    // blendMode: 描画に使用するブレンドモード（デフォルト Alpha）
    // depth: 深度の扱い。None は、ボリューム雲の合成のあと(描画先が色だけになる)に描くときに使う。
    //        TestAndWrite は、雲の合成の前に描く粒用(雲の合成が深度を見るので、粒の位置までの雲だけが粒にかかり、雲のもやで自然にかすむ)
    void Draw(BlendMode blendMode = BlendMode::Alpha, GPUParticleDepth depth = GPUParticleDepth::Test);
    // 発光パス: 同じ粒を、加算で、熱さに応じた色(赤、橙、黄、白)・速度の向きの尾・ちらつきつきで描く。
    // ボリューム雲の合成のあと(描画先が色だけ)に呼ぶ。深度は depthSrv(シーンの深度の SRV)を読んで、島などの後ろの粒を隠す。
    // 更新(Update)は Draw 側で済んでいること(同じフレームで、Draw の後に呼ぶ)
    void DrawGlow(const GlowParams& params, D3D12_GPU_DESCRIPTOR_HANDLE depthSrv);

    // --- エミッタ操作API（加点要素: GPU Particle拡張） ---

    using EmitterShape = GPUEmitterShape;

    // 粒の見た目と動きの出だしを決めるスタイル。エミッタに付けると、粒の色・大きさ・速さ・寿命を、ここで指定した範囲で決める
    // (付けないエミッタは、従来どおり、色も大きさも速さも寿命もランダム)。鱗粉のように、色を決めたい演出が使う
    struct ParticleStyle {
        Vector3 colorStart{ 1.0f, 1.0f, 1.0f }; // 生まれたときの色
        Vector3 colorEnd{ 1.0f, 1.0f, 1.0f };   // 冷めきったときの色
        float coolRatio = 0.6f;                 // 色が変わりきる寿命の割合(0 〜 1)
        float fadePower = 2.0f;                 // アルファ = 1 - (経過割合 ^ fadePower)。大きいほど終わり際まで残る
        float lifeMin = 1.0f;                   // 寿命(秒)の範囲
        float lifeMax = 2.0f;
        float sizeMin = 0.05f;                  // 大きさ(ワールド単位)の範囲
        float sizeMax = 0.1f;
        float speedMin = 0.0f;                  // 発生時の速さ(向きはランダム)の範囲
        float speedMax = 1.0f;
        Vector3 baseVelocity{ 0.0f, 0.0f, 0.0f }; // 全ての粒に足す速度(発生源の速度の引き継ぎなど)
        bool glowOnly = false;                  // true なら、通常の描画では描かず、発光パス(DrawGlow)だけで描く(核の光など)
        float glowGain = 1.0f;                  // 発光パスでの明るさの倍率
        bool glowOwnColor = false;              // true なら、発光パスで、熱さの色(橙・黄)ではなく、粒自身の色(colorStart から colorEnd へ変わる色)で光らせる
    };


    struct EmitterDesc {
        EmitterShape shape = EmitterShape::Sphere;
        Vector3 translate{ 0.0f, 0.0f, 0.0f };
        Vector3 halfExtents{ 1.0f, 1.0f, 1.0f }; // Box用
        float radius = 1.0f;                     // Sphere用
        uint32_t count = 10;
        float frequency = 0.5f; // 0以下なら自動ループしない（SetEmitterEnabledで手動制御する想定）
        bool styled = false;    // true なら style を使う
        ParticleStyle style;
    };

    // 常駐エミッタを1つ生成し、そのインデックスを返す（環境演出用）。
    // 予約数を超えた場合は失敗し UINT32_MAX を返す。ReleaseEmitter で返したスロットは、再利用される
    uint32_t CreateEmitter(const EmitterDesc& desc);
    // 常駐エミッタを返す(止めて、スロットを空ける)。シーンの終わりに、作ったエミッタを返すために使う
    void ReleaseEmitter(uint32_t emitterIndex);
    // 作った常駐エミッタの設定を、まるごと置き換える(射出のタイマーは保つ)。毎フレーム呼んでもよい
    void ConfigureEmitter(uint32_t emitterIndex, const EmitterDesc& desc);
    void SetEmitterTransform(uint32_t emitterIndex, const Vector3& translate);
    void SetEmitterEnabled(uint32_t emitterIndex, bool enabled);

    // 内部で予約されたバーストプールをラウンドロビンし、指定位置へワンショットのパーティクル群を発生させる。
    void EmitBurst(const Vector3& position, uint32_t count = 15, float radius = 0.3f);
    // スタイル付きのバースト。1 フレームに出せるバーストは kBurstPoolSize 個まで(超えると、古い方の指定が上書きされる)
    void EmitStyledBurst(const Vector3& position, uint32_t count, float radius, const ParticleStyle& style);
    // 全ての粒を消して、空きの管理も最初の状態に戻す(次の Update で行う)。前の演出の残りの粒が、次の演出の開始時に出ないようにするために使う
    void ClearAllParticles() { clearAllPending_ = true; }
    // 1 フレームに出せるバーストの数
    static constexpr uint32_t GetBurstPoolSize() { return kBurstPoolSize; }

    using FieldType = GPUFieldType;

    struct FieldDesc {
        FieldType type = FieldType::None;
        Vector3 target{ 0.0f, 0.0f, 0.0f };
        Vector3 direction{ 0.0f, 1.0f, 0.0f };
        float strength = 1.0f;
    };

    // slot は [0, kMaxGPUFields) の範囲で指定する。
    void SetField(int slot, const FieldDesc& desc);

private:
    GPUParticleManager() = default;
    ~GPUParticleManager() = default;

    void CreateResources();
    void CreateComputePipeline();
    void CreateGraphicsPipeline();
    void CreateQuad();
    void CreateGlowPipeline();
    // エミッタのスロットへ、desc の内容を書き込む(射出のタイマー frequencyTime と emit は触らない)
    static void WriteEmitterDesc_(GPUEmitter& em, const EmitterDesc& desc);

    DirectXCommon* dx_ = nullptr;

    // Resource
    ComPtr<ID3D12Resource> particleBuffer_;
    uint32_t srvIndex_ = 0;
    uint32_t uavIndex_ = 0;

    // Pipelines
    ComPtr<ID3D12RootSignature> computeRootSignature_;
    ComPtr<ID3D12PipelineState> computePipelineState_;

    // Emit/Update共有（b0:Emitter[], b1:PerFrame, b2:Field[], u0-u2）。
    // フレーム内でRootSignatureを切り替えるとD3D12デバッグレイヤーのクラッシュを誘発するため、
    // Emit/Updateで1つのRootSignatureを共有し、PSOのみを切り替える構成にしている。
    ComPtr<ID3D12RootSignature> emitRootSignature_;
    ComPtr<ID3D12PipelineState> emitPipelineState_;
    ComPtr<ID3D12PipelineState> updatePipelineState_;

    ComPtr<ID3D12RootSignature> graphicsRootSignature_;
    // ブレンドモードごとのパイプライン（インデックスは BlendMode の enum 値と対応）
    // [0]=Opaque相当, [1]=Alpha, [2]=Add, [3]=Subtract, [4]=Multiply, [5]=Screen
    static constexpr int kBlendModeCount = 6;
    ComPtr<ID3D12PipelineState> graphicsPipelineStates_[kBlendModeCount];
    // 深度なし(色だけの描画先用)。ボリューム雲の合成のあとに描くときに使う
    ComPtr<ID3D12PipelineState> graphicsPipelineStatesColorOnly_[kBlendModeCount];
    // 深度テストあり・書き込みあり。雲の合成の前に描く粒用
    ComPtr<ID3D12PipelineState> graphicsPipelineStatesDepthWrite_[kBlendModeCount];

    // 発光パス(雲のあとに加算で描く)用
    ComPtr<ID3D12RootSignature> glowRootSignature_;
    ComPtr<ID3D12PipelineState> glowPipelineState_;
    ComPtr<ID3D12Resource> glowParamCB_;
    GlowParams* glowParamMapped_ = nullptr;

    // Constant Buffer for PerView
    struct PerView {
        Matrix4x4 viewProjection;
        Matrix4x4 billboardMatrix;
    };
    ComPtr<ID3D12Resource> perViewCB_;
    PerView* perViewMapped_ = nullptr;

    struct PerFrame {
        float time;
        float deltaTime;
        uint32_t clearAll; // 0 以外のフレームは、Emit は何もせず、Update が全ての粒を消して空きの管理を戻す
        float pad;
    };
    ComPtr<ID3D12Resource> perFrameCB_;
    PerFrame* perFrameMapped_ = nullptr;

    ComPtr<ID3D12Resource> emittersCB_;
    GPUEmitterArray* emittersMapped_ = nullptr;

    ComPtr<ID3D12Resource> fieldsCB_;
    GPUFieldArray* fieldsMapped_ = nullptr;

    // エミッタスロットの割り当て: [0, kPersistentEmitterSlots) は CreateEmitter の常駐用、
    // [kPersistentEmitterSlots, kMaxGPUEmitters) は EmitBurst のラウンドロビン用プール。
    static constexpr uint32_t kBurstPoolSize = 4;
    static constexpr uint32_t kPersistentEmitterSlots = kMaxGPUEmitters - kBurstPoolSize;
    uint32_t persistentEmitterCount_ = 0; // 使い始めたスロットの数(返されたスロットも含む)
    bool persistentEmitterFree_[kPersistentEmitterSlots] = {}; // ReleaseEmitter で返されて、再利用できるスロット
    uint32_t burstCursor_ = 0;
    bool clearAllPending_ = false; // ClearAllParticles が呼ばれて、次の Update で消す
    // EmitBurstで立てたemitフラグは「立てた次のUpdate()」でクリアする(GPUが前フレームの
    // Dispatchを消費し終えているタイミングで安全にクリアするため)。
    bool burstPendingClear_[kBurstPoolSize] = {};
    float timeScale_ = 1.0f; // 粒の動きの時間の速さ(SetTimeScale)

    ComPtr<ID3D12Resource> freeListIndexBuffer_;
    ComPtr<ID3D12Resource> freeListBuffer_;
    uint32_t freeListIndexUavIndex_ = 0;
    uint32_t freeListUavIndex_ = 0;

    std::unique_ptr<TextureResource> texture_;

    // Quad Mesh
    ComPtr<ID3D12Resource> vb_;
    ComPtr<ID3D12Resource> ib_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    static const uint32_t kMaxParticles = 1024;
};
