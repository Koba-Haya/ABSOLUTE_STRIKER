#include "ModelComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include "DissolveComponent.h"
#include "../../resources/AssetManager.h"
#include "../graphics/3d/model/ModelResource.h"
#include "Method.h"
#include "AnimationManager.h"
#include "Logger.h"
#include <algorithm>
#include <optional>
#include <vector>
#include "CpuTimer.h"

namespace AbsoluteEngine {

namespace {
constexpr float kMinFitWidth = 1e-4f; // これより幅が小さいモデルは、拡大率を求めない(0 割りを避ける)
} // namespace

void ModelComponent::LoadModel(const std::string& path) {
    // 停止の調査用: リソース取得(キャッシュ命中なら速い)と、インスタンス生成を分けて測る
    std::optional<CpuTimer::AccumScope> resScope;
    resScope.emplace("ModelComponent.LoadResource");
    auto res = AssetManager::GetInstance()->Load<ModelResource>(path);
    resScope.reset();
    if (!res) return;

    CpuTimer::AccumScope instScope("ModelComponent.CreateInstance");
    modelPath_ = path;
    animationSet_.reset(); // 別のファイルのアニメーションを引かないよう、モデルを替えたら読み直す
    fitCenter_ = {0.0f, 0.0f, 0.0f};
    fitScale_ = 1.0f;
    // 既存のmodelInstance_をここで即座に破棄しない。読み込み直後にLoadModel()が
    // 連続で呼ばれるケース（例：シーンJSONでロードされたモデルを起動直後に別モデルへ
    // 強制差し替えする処理）では、直前のモデルのテクスチャアップロードコマンドがまだ
    // クローズされていないコマンドリストに記録されたままのことがある。そこで即解放すると
    // D3D12がその場でリソースを破棄し、Close()時にOBJECT_DELETED_WHILE_STILL_IN_USEで
    // クラッシュする。次のUpdate()（＝次のフレーム、EndFrame()でのフェンス待機後）まで
    // 解放を遅延させることで、コマンドリストが安全に消費された後に破棄する。
    pendingDestroyModelInstance_ = std::move(modelInstance_);
    modelInstance_ = std::make_unique<ModelInstance>();
    ModelInstance::CreateInfo ci{};
    ci.resource = res;
    ci.baseColor = {color_.x, color_.y, color_.z, color_.w};
    ci.lightingMode = lightingMode_;
    ci.environmentCoefficient = environmentCoefficient_;
    modelInstance_->Initialize(ci);
    
    if (!texturePath_.empty()) {
        LoadTexture(texturePath_);
    }
    if (dissolveMask_) {
        modelInstance_->SetDissolveMaskTexture(dissolveMask_);
    }
}

void ModelComponent::FitToWingspan(float wingspan) {
    if (!modelInstance_) return;
    const ModelResource* resource = modelInstance_->GetResource();
    if (!resource) return;
    const std::vector<Vector3>& positions = resource->GetVertexPositions();
    if (positions.empty()) return;

    // バウンディングボックスの最小・最大
    Vector3 lo = positions[0];
    Vector3 hi = positions[0];
    for (const Vector3& p : positions) {
        lo = {(std::min)(lo.x, p.x), (std::min)(lo.y, p.y), (std::min)(lo.z, p.z)};
        hi = {(std::max)(hi.x, p.x), (std::max)(hi.y, p.y), (std::max)(hi.z, p.z)};
    }
    fitCenter_ = {(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    const float width = hi.x - lo.x;
    fitScale_ = width > kMinFitWidth ? wingspan / width : 1.0f;
}

void ModelComponent::LoadDissolveMask(const std::string& path) {
    dissolveMask_ = AssetManager::GetInstance()->Load<TextureResource>(path);
    if (modelInstance_ && dissolveMask_) {
        modelInstance_->SetDissolveMaskTexture(dissolveMask_);
    }
}

void ModelComponent::LoadTexture(const std::string& path) {
    texturePath_ = path;
    if (modelInstance_) {
        auto tex = AssetManager::GetInstance()->Load<TextureResource>(path);
        if (tex) {
            modelInstance_->SetOverrideTexture(tex);
        }
    }
}

void ModelComponent::PlayAnimation(const std::string& animationName, bool loop, float blendSeconds) {
    if (!modelInstance_) return;

    if (!animationSet_) {
        // "dir/file.gltf" を、ディレクトリとファイル名に分けて AnimationManager へ渡す
        const size_t slash = modelPath_.find_last_of('/');
        const std::string directory = slash == std::string::npos ? std::string{} : modelPath_.substr(0, slash);
        const std::string filename = slash == std::string::npos ? modelPath_ : modelPath_.substr(slash + 1);
        animationSet_ = AnimationManager::GetInstance()->LoadAnimationSet(directory, filename);
    }
    const std::shared_ptr<Animation> animation = animationSet_ ? animationSet_->Find(animationName) : nullptr;
    if (!animation) {
        Logger::GetInstance().Log("[ModelComponent] animation not found: model=%s name=%s", modelPath_.c_str(), animationName.c_str());
        return;
    }
    modelInstance_->PlayAnimation(animation, loop, blendSeconds);
}

bool ModelComponent::IsAnimationFinished() const {
    return modelInstance_ && modelInstance_->IsAnimationFinished();
}

bool ModelComponent::TryGetBoneWorldPosition(const std::string& boneName, Vector3& out) const {
    if (!modelInstance_ || !owner_) return false;
    Matrix4x4 boneModel;
    if (!modelInstance_->TryGetBoneModelMatrix(boneName, boneModel)) return false;
    const Matrix4x4 boneWorld = Multiply(boneModel, ComputeWorldMatrix_());
    out = {boneWorld.m[3][0], boneWorld.m[3][1], boneWorld.m[3][2]};
    return true;
}

Matrix4x4 ModelComponent::ComputeWorldMatrix_() const {
    const Transform& transform = owner_->GetTransform();
    // 行ベクトル規約なので左から順に適用: 中心を原点へ → 補正の拡大 → Transform(拡大・回転・位置)
    const Matrix4x4 fit = Multiply(MakeTranslateMatrix({-fitCenter_.x, -fitCenter_.y, -fitCenter_.z}),
                                   MakeScaleMatrix({fitScale_, fitScale_, fitScale_}));
    return Multiply(fit, MakeAffineMatrix(transform.scale, transform.rotate, transform.translate));
}

void ModelComponent::Update(float deltaTime) {
    // 前フレームまでに置き換えられた古いモデルをここで解放する。
    // このUpdate()が呼ばれる時点で前フレームのコマンドリストはEndFrame()の
    // フェンス待機によって実行完了済みのため、安全に破棄できる。
    pendingDestroyModelInstance_.reset();

    if (modelInstance_) {
        modelInstance_->UpdateAnimation(deltaTime);
    }
}

void ModelComponent::Draw() {
    if (modelInstance_ && owner_) {
        modelInstance_->SetWorld(ComputeWorldMatrix_());

        auto dissolve = owner_->GetComponent<DissolveComponent>();
        if (dissolve) {
            modelInstance_->SetDissolveParam(dissolve->enable, dissolve->threshold, dissolve->edgeRange, dissolve->edgeColor, dissolve->maskColor);
        }
        
        modelInstance_->Draw();
    }
}

} // namespace AbsoluteEngine
