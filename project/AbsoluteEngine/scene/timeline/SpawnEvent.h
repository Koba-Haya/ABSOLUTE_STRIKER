// ============================================================
// SpawnEvent.h
// 役割: プレハブIDで指定されたGameObjectをスポーンするタイムラインイベント
//       生成したオブジェクトは weak_ptr で保持し、Rewind時に安全に破棄する
// ============================================================
#pragma once
#include "SpawnTimelineEvent.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {

class GameObject;

// プレハブを 1 体出すイベント。置き場所などのスポーン系に共通の設定と処理は、中間クラス SpawnTimelineEvent が持つ
class SpawnEvent final : public SpawnTimelineEvent {
public:
    SpawnEvent() = default;
    // デストラクタ: unique_ptr による消滅時（トラック削除など）に
    // プレビューオブジェクトが確実に破棄されるよう Rewind() を呼ぶ
    ~SpawnEvent() override { Rewind(); }

    // -----------------------------------------------------------
    // ITimelineEvent 実装
    // -----------------------------------------------------------

    std::string GetLabel() const override;

    // PrefabRegistryからパスを解決し、プレハブをスポーンして rootObjects に追加する
    // スポーンしたオブジェクトは weak_ptr で保持する
    void Fire(const TimelineFireContext& context) override;

    // weak_ptr 経由でオブジェクトの生存を確認してから安全に破棄する
    // （ダングリングポインタによるクラッシュを100%防ぐ）
    void Rewind() override;

    std::string GetEventType() const override { return "SpawnEvent"; }
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    void DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) override;

    // 作成時のみ、プレハブIDを選択可能なコンボとして描画する
    // （DrawInspectorUI()では配置後の誤操作防止のため表示のみ・変更不可にしている）
    void DrawCreationUI(bool& activated, bool& deactivatedAfterEdit) override;

private:
    // Fire()でスポーンしたオブジェクトへの弱参照
    // shared_ptr を直接持つと所有権の衝突が起きるため weak_ptr を使用する
    std::weak_ptr<GameObject> spawnedObject_;
};

} // namespace AbsoluteEngine
