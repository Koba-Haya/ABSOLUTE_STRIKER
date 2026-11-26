#pragma once
#include <memory>
#include <string>
#include "../../externals/nlohmann/json.hpp" // JSONライブラリのインクルード

namespace AbsoluteEngine {

class GameObject;
class IEditRecorder;
struct RailFrame; // レール上の座標系(ITimelineCamera.h で定義)

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    // 雲の合成のあとに描きたいものがあるとき、ここで描く(Draw とは別。シーンが、雲を合成したあとに呼ぶ)。
    // 雲に隠れさせたくない演出(弾の光など)用。このときの描画先は色だけで、深度は見ない
    virtual void DrawAfterClouds() {}
    virtual void OnCollision(GameObject* other) {}

    // タイムラインのイベントが、レール座標の指定でこのオブジェクトを出現させた直後に呼ばれる。
    // frame は、出現位置のレールの座標系(frame.forward がレールの進行方向 = 自機側から見て奥へ向かう向き)。
    // レールの向きに依存する動き(進行方向と逆へ近づく、など)は、ここで自分の向きを決める。
    // 世界座標で指定された出現では呼ばれず、各コンポーネントの既定の向きのまま動く
    // 基底に置いてよい理由: これは「出現した」という通知で、OnCollision と同じ種類のもの。受け取りたい部品だけが使い、
    // ほかの部品は無視するのが正しい振る舞い(何もしないことに意味がある)。「できない機能を持たされる」もの
    // (例: 飛べないペンギンの Fly)ではないので、中間クラスへ分けない
    virtual void OnSpawnedOnRail(const RailFrame& frame) { (void)frame; }
    
    // コンポーネントの種類名（ファクトリやシリアライズ用）
    virtual std::string GetTypeName() const = 0;

    // エディタ（インスペクタ）用UI描画
    virtual void DrawInspectorUI() {}
    // エディタ（インスペクタ）用UI描画(編集を取り消し履歴へ記録したい部品用)。
    // エディタは、こちらを呼ぶ。recorder へ「編集した」と伝えると、エディタが履歴を作る。
    // 既定では、記録せず、引数なしの版を呼ぶだけ(記録の要らない部品は、引数なしの版だけを実装すればよい)
    virtual void DrawInspectorUI(IEditRecorder& recorder) { DrawInspectorUI(); }

    // シリアライズ（保存・復元）用インターフェース
    virtual void Serialize(nlohmann::json& j) const {}
    virtual void Deserialize(const nlohmann::json& j) {}

    void SetOwner(GameObject* owner) { owner_ = owner; }
    GameObject* GetOwner() const { return owner_; }

private:
    // このコンポーネントを持つオブジェクト。派生クラスからは GetOwner() で読むだけにし、
    // 書き換えは、オブジェクトへ取り付けるときの SetOwner だけにする
    GameObject* owner_ = nullptr;
};

} // namespace AbsoluteEngine
