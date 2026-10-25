#pragma once
#include "Component.h"
#include "../graphics/3d/model/ModelInstance.h"
#include "../graphics/3d/model/Animation.h"
#include "Type/Vector.h"
#include <string>
#include <memory>

namespace AbsoluteEngine {

class ModelComponent : public IComponent {
public:
    ModelComponent() = default;
    ~ModelComponent() override = default;

    std::string GetTypeName() const override { return "ModelComponent"; }

    void Serialize(nlohmann::json& j) const override {
        j["modelPath"] = modelPath_;
        j["texturePath"] = texturePath_;
        j["environmentCoefficient"] = environmentCoefficient_;
        j["color"] = { {"x", color_.x}, {"y", color_.y}, {"z", color_.z}, {"w", color_.w} };
    }
    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("modelPath")) {
            std::string path = j["modelPath"].get<std::string>();
            if (!path.empty() && path != "/") LoadModel(path);
        }
        if (j.contains("texturePath")) {
            std::string path = j["texturePath"].get<std::string>();
            if (!path.empty() && path != "/") LoadTexture(path);
        }
        if (j.contains("environmentCoefficient")) {
            SetEnvironmentCoefficient(j["environmentCoefficient"].get<float>());
        }
        // 敵種別を見た目で区別するための色味（テクスチャを増やさずデータ駆動で差をつける）
        if (j.contains("color")) {
            const auto& c = j["color"];
            SetColor({ c.value("x", 1.0f), c.value("y", 1.0f), c.value("z", 1.0f), c.value("w", 1.0f) });
        }
    }

    void LoadModel(const std::string& path);
    void LoadTexture(const std::string& path);
    // このモデル専用のディゾルブのマスクを読み込む(モデルの UV で引かれる)。LoadModel() で作り直しても保持される
    void LoadDissolveMask(const std::string& path);

    // 照明モード(ModelLightingMode)。既定は Lambert。LoadModel() で作り直しても保持される
    void SetLightingMode(int32_t mode) {
        lightingMode_ = mode;
        if (modelInstance_) {
            modelInstance_->SetLightingMode(mode);
        }
    }

    const std::string& GetModelPath() const { return modelPath_; }
    const std::string& GetTexturePath() const { return texturePath_; }

    ModelInstance* GetModelInstance() const { return modelInstance_.get(); }

    // モデルのファイル内のアニメーションを、名前で選んで再生する(アニメーションが 1 ファイルに複数入っているモデル用)。
    // 読み込みは初めて呼ばれたときに 1 回だけ行う。モデル未読み込み、または名前が無いときは、ログを出して何もしない。
    // blendSeconds > 0 なら、いまのポーズから、その秒数でクロスフェードする
    void PlayAnimation(const std::string& animationName, bool loop = true, float blendSeconds = 0.0f);
    // ループしないアニメーションが最後まで再生されたか(ModelInstance::IsAnimationFinished と同じ)
    bool IsAnimationFinished() const;

    // ボーンの「今の」ワールド座標を返す(Transform と FitToWingspan の補正を反映する)。ボーンが無ければ false
    bool TryGetBoneWorldPosition(const std::string& boneName, Vector3& out) const;

    // モデル空間からワールドへの、今の行列(Transform と FitToWingspan の補正を反映する)。CPU でスキニングした頂点を、ワールドへ移すのに使う
    Matrix4x4 GetWorldMatrix() const { return owner_ ? ComputeWorldMatrix_() : MakeIdentity4x4(); }

    // 読み込んだモデルの翼幅(x 方向の幅)が wingspan になるよう拡大し、バウンディングボックスの中心を
    // オブジェクトの位置に合わせる。Transform の scale / rotate / translate とは別に、モデルの空間で掛かる補正。
    // モデルを差し替えても大きさと中心がそろう。モデルが読み込まれていなければ何もしない。
    // 注意: LoadModel() のあとに呼ぶこと(読み込み直すと補正は初期化される)
    void FitToWingspan(float wingspan);

    void SetEnvironmentCoefficient(float c) {
        environmentCoefficient_ = c;
        if (modelInstance_) {
            modelInstance_->SetEnvironmentCoefficient(c);
        }
    }
    float GetEnvironmentCoefficient() const { return environmentCoefficient_; }

    // モデルの単色ティント（RGBA）。LoadModel()で作り直しても保持される。
    void SetColor(const Vector4& c) {
        color_ = c;
        if (modelInstance_) {
            modelInstance_->SetColor(c);
        }
    }
    const Vector4& GetColor() const { return color_; }

    void Update(float deltaTime) override;
    void Draw() override;

private:
    // FitToWingspan の補正と Transform を合わせた、今のワールド行列(Draw と TryGetBoneWorldPosition で共通)
    Matrix4x4 ComputeWorldMatrix_() const;

    std::string modelPath_;
    std::string texturePath_;
    std::unique_ptr<ModelInstance> modelInstance_;
    // LoadModel()で置き換えられた直前のモデル（とそのテクスチャ）を1フレームだけ延命させる。
    // 同一フレーム内（コマンドリストがCloseされる前）に破棄すると、直前のロード処理で
    // 記録されたテクスチャアップロードコマンドが参照するリソースが消え、
    // D3D12 ERROR: OBJECT_DELETED_WHILE_STILL_IN_USE でクラッシュするため。
    std::unique_ptr<ModelInstance> pendingDestroyModelInstance_;
    // modelPath_ のファイルに入っているアニメーションの集合(PlayAnimation で初めて必要になったときに読む)
    std::shared_ptr<const AnimationSet> animationSet_;
    // このモデル専用のディゾルブのマスク(空なら、共有のマスクを使う)
    std::shared_ptr<TextureResource> dissolveMask_;
    float environmentCoefficient_ = 0.0f;
    int32_t lightingMode_ = ModelLightingMode::kLambert; // 照明モード(SetLightingMode)
    Vector4 color_{1.0f, 1.0f, 1.0f, 1.0f};
    // FitToWingspan の補正(モデル空間)。既定は補正なし
    Vector3 fitCenter_{0.0f, 0.0f, 0.0f};
    float fitScale_ = 1.0f;
};

} // namespace AbsoluteEngine
