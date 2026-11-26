#pragma once
#include "Component.h"
#include "Transform.h"
#include "../graphics/3d/model/ModelInstance.h"
#include <string>

namespace AbsoluteEngine {

class DissolveComponent : public IComponent {
public:
    // ディゾルブ(モデルの表面が、マスクに沿って消えていく演出)の設定値の束(値を並べただけのデータの入れ物)。
    // 外へ出すときは、この束ごと、読み取り(GetSettings)と、書き換え(SetSettings)の 1 つずつの窓口を通す
    struct Settings {
        bool enable = false;
        float threshold = 0.5f;
        float edgeRange = 0.03f;
        Vector3 edgeColor = {1.0f, 0.4f, 0.3f};
        Vector3 maskColor = {1.0f, 1.0f, 1.0f};
        float emberAmount = 0.0f; // 縁を熾火(灰 -> 赤 -> 橙)で光らせる強さ(0 〜 1。0 は従来どおり)
    };

    DissolveComponent() = default;
    ~DissolveComponent() override = default;

    std::string GetTypeName() const override { return "DissolveComponent"; }

    // ディゾルブの設定値を読み取り専用で返す。書き換えるときは、コピーを編集して SetSettings で戻す
    const Settings& GetSettings() const { return settings_; }
    // ディゾルブの設定値を丸ごと差し替える(書き込みの窓口は、SetCrack と、ここだけ)
    void SetSettings(const Settings& settings) { settings_ = settings; }

    // エディタのインスペクタに、ディゾルブの設定を表示し、編集を受け付ける。
    // 注意: この部品は Serialize を持たないので、ここでの編集は、取り消し履歴にもシーンのファイルにも残らない(調整の確認用)
    void DrawInspectorUI() override;

    // ボロノイのひび割れ(ディゾルブと同じく、「モデルの表面が消えていく」演出のシェーダー効果なので、ここにまとめて持つ)。
    // crack.enable が false なら、何も描かない。ModelComponent が、毎フレーム、モデルへ反映する
    const ModelInstance::CrackParam& GetCrack() const { return crack_; }
    void SetCrack(const ModelInstance::CrackParam& crack) { crack_ = crack; }

private:
    Settings settings_;                // ディゾルブの設定値
    ModelInstance::CrackParam crack_;  // ひび割れの設定値
};

} // namespace AbsoluteEngine
