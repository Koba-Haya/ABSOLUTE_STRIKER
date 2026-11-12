#pragma once
#include <memory>
#include <string>
#include "../../externals/nlohmann/json.hpp" // JSONライブラリのインクルード

namespace AbsoluteEngine {

class GameObject;
struct RailFrame; // レール上の座標系(ITimelineCamera.h で定義)

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    virtual void OnCollision(GameObject* other) {}

    // タイムラインのイベントが、レール座標の指定でこのオブジェクトを出現させた直後に呼ばれる。
    // frame は、出現位置のレールの座標系(frame.forward がレールの進行方向 = 自機側から見て奥へ向かう向き)。
    // レールの向きに依存する動き(進行方向と逆へ近づく、など)は、ここで自分の向きを決める。
    // 世界座標で指定された出現では呼ばれず、各コンポーネントの既定の向きのまま動く
    virtual void OnSpawnedOnRail(const RailFrame& frame) {}
    
    // コンポーネントの種類名（ファクトリやシリアライズ用）
    virtual std::string GetTypeName() const = 0;

    // エディタ（インスペクタ）用UI描画
    virtual void DrawInspectorUI() {}

    // シリアライズ（保存・復元）用インターフェース
    virtual void Serialize(nlohmann::json& j) const {}
    virtual void Deserialize(const nlohmann::json& j) {}

    void SetOwner(GameObject* owner) { owner_ = owner; }
    GameObject* GetOwner() const { return owner_; }

protected:
    GameObject* owner_ = nullptr;
};

} // namespace AbsoluteEngine
