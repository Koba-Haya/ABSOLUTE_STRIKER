#pragma once

#include "AABB.h"
#include "Matrix.h"
#include "SrvHandle.h"
#include "Transform.h"
#include "Vector.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

class DirectXCommon;
class TextureResource;
#include "UnifiedPipeline.h"

// 加速度フィールド（範囲内の粒子に加速度を与える）
struct AccelerationField {
    Vector3 acceleration;
    AABB area;
};

// パーティクル（CPU側）
struct Particle {
    Transform transform;
    Vector3 velocity;
    float lifetime = 1.0f;
    float age = 0.0f;
    Vector4 color{ 1,1,1,1 };
    // 寿命の間に transform.scale から endScale へ線形に変える(changesScale が true のときだけ)
    Vector3 endScale{ 1,1,1 };
    bool changesScale = false;
};

// GPUへ送るインスタンシングデータ
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

// PS側 Material(b0)
struct ParticleMaterialData {
    Vector4 color;
    int32_t enableLighting;
    float pad[3];
    Matrix4x4 uvTransform;
};

// パーティクルシステム全体を管理する（SRV/StructuredBuffer/描画/更新）
// - グループ単位（テクスチャ単位）でインスタンシング描画
// - Emit は外部（Emitter等）から呼ぶ
class ParticleManager {
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    static ParticleManager* GetInstance();

    // DX関連を受け取って初期化
    bool Initialize(DirectXCommon* dx);
    void Finalize();

    // テクスチャ単位のグループを作成
    // - name: グループ名（ユーザーが付けるキー）
    // - texturePath: TextureManagerで読むファイルパス
    // - maxInstances: StructuredBuffer の最大要素数
    // - softAlpha: true にすると「柔らかいアルファ」のグループになる(霧・雲など、縁がなだらかに重なって見えるもの)。
    //   Draw の filter で、通常のグループ(加算などで描く)と別のタイミング・別のブレンドで描き分けられる
    bool CreateParticleGroup(const std::string& name, const std::string& texturePath, uint32_t maxInstances, bool softAlpha = false);

    // パーティクルをグループに発生させる（Emitterから呼ぶ想定）
    // endScale を渡すと、寿命の間に scale から endScale へ大きさを変える(煙が広がるなど)。省略時は一定
    void Emit(const std::string& name, const Vector3& position, const Vector3& velocity,
        const Vector3& scale, const Vector3& rotate, float lifetime, const Vector4& color,
        const Vector3* endScale = nullptr);

    // Update: 粒子更新 + インスタンスデータ書き込み
    void Update(float deltaTime);

    // Draw: グループごとに DrawIndexedInstanced
    // depthTest: true なら深度テストあり(書き込みなし)で描く。他の 3D オブジェクトとの前後関係が正しく出る。
    //            Alpha ブレンドのみ対応(他のブレンドでは無視される)。既定の false は従来どおり深度を見ない
    // filter: 描くグループの選び方。既定はすべて(従来どおり)。
    //         「柔らかいアルファ」のグループだけ別に描きたいときは、通常の Draw に NormalOnly、柔らかいほうに SoftAlphaOnly を渡す
    void Draw(BlendMode blendMode = BlendMode::Alpha, bool depthTest = false, ParticleDrawFilter filter = ParticleDrawFilter::All);

    // Called by Renderer
    void DrawInternal(ID3D12GraphicsCommandList* cmdList, ParticleDrawFilter filter = ParticleDrawFilter::All);

    // パラメータ
    void SetAccelerationField(const AccelerationField& field) { accelerationField_ = field; }
    void SetEnableAccelerationField(bool enable) { enableAccelerationField_ = enable; }

    // グループの最大インスタンス（UIで減らす等に使う）
    void SetGroupInstanceLimit(const std::string& name, uint32_t limit);

	// グループ内の全粒子をクリア
    void ClearParticleGroup(const std::string& name);

    // グループ内の全粒子を delta だけ平行移動する(描画用のワールド座標を巻き戻したとき、残っている粒子を合わせる)
    void TranslateParticleGroup(const std::string& name, const Vector3& delta);

private:
    ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    Matrix4x4 MakeBillboardMatrix_(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Matrix4x4& viewMatrix) const;
    bool IsCollision_(const AABB& aabb, const Vector3& point) const;

    void EnsureQuadGeometry_();

private:
    struct ParticleGroup {
        ~ParticleGroup() {
            if (instanceMapped && instanceBuffer) {
                instanceBuffer->Unmap(0, nullptr);
                instanceMapped = nullptr;
            }
            if (materialMapped && materialCB) {
                materialCB->Unmap(0, nullptr);
                materialMapped = nullptr;
            }
            instanceSrv.Reset();
        }

        ParticleGroup() = default;
        ParticleGroup(const ParticleGroup&) = delete;
        ParticleGroup& operator=(const ParticleGroup&) = delete;

        ParticleGroup(ParticleGroup&& other) noexcept
            : texture(std::move(other.texture)),
              textureSrvGpu(other.textureSrvGpu),
              particles(std::move(other.particles)),
              maxInstances(other.maxInstances),
              instanceLimit(other.instanceLimit),
              activeInstanceCount(other.activeInstanceCount),
              instanceBuffer(std::move(other.instanceBuffer)),
              instanceMapped(other.instanceMapped),
              instanceSrv(std::move(other.instanceSrv)),
              instanceSrvGpu(other.instanceSrvGpu),
              materialCB(std::move(other.materialCB)),
              materialMapped(other.materialMapped),
              softAlpha(other.softAlpha) {
            other.textureSrvGpu = {};
            other.maxInstances = 0;
            other.instanceLimit = 0;
            other.activeInstanceCount = 0;
            other.instanceMapped = nullptr;
            other.instanceSrvGpu = {};
            other.materialMapped = nullptr;
        }

        ParticleGroup& operator=(ParticleGroup&& other) noexcept {
            if (this != &other) {
                this->~ParticleGroup();
                new (this) ParticleGroup(std::move(other));
            }
            return *this;
        }

        std::shared_ptr<TextureResource> texture;
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGpu{};

        std::list<Particle> particles;

        uint32_t maxInstances = 0;
        uint32_t instanceLimit = 0;
        uint32_t activeInstanceCount = 0;

        // StructuredBuffer (t1)
        ComPtr<ID3D12Resource> instanceBuffer;
        ParticleForGPU* instanceMapped = nullptr;
        SrvHandle instanceSrv;
        D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvGpu{};

        // Material (b0)
        ComPtr<ID3D12Resource> materialCB;
        ParticleMaterialData* materialMapped = nullptr;

        // 「柔らかいアルファ」のグループか(Draw の filter で選ぶ)。メンバ宣言の順は、上のムーブコンストラクタの初期化の順と合わせる
        bool softAlpha = false;
    };

    DirectXCommon* dx_ = nullptr;
    ComPtr<ID3D12Device> device_;

    std::unordered_map<std::string, ParticleGroup> groups_;

    // 共通の板ポリ
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};
    ComPtr<ID3D12Resource> vb_;
    ComPtr<ID3D12Resource> ib_;
    bool quadReady_ = false;

    AccelerationField accelerationField_{};
    bool enableAccelerationField_ = false;
};
