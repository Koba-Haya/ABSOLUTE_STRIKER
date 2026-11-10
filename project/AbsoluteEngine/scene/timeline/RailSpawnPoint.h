// ============================================================
// RailSpawnPoint.h
// 役割: 出現位置を「レール座標」で指定するための型と、その変換・保存・編集用の関数。
//       レール座標 = (前方 ahead, 横 lateral, 縦 vertical)。
//         ahead   : 発火時刻のカメラ位置から、レールに沿って何ユニット先か
//         lateral : そこから、道の右へ何ユニットか(負なら左)
//         vertical: そこから、道の上へ何ユニットか(負なら下)
//       発火の瞬間に、レールの座標系(GetRailFrame)を使って世界座標へ変換する。出現した後は世界座標で動く。
//
// なぜ別ファイルか: SpawnEvent(エンジン層)と FormationSpawnEvent(ゲーム層)の両方が、同じ指定・同じ変換・
//       同じ保存形式・同じ編集 UI を使う。重複を避けるため、1 責務(レール座標での出現位置)として切り出した。
// ============================================================
#pragma once
#include "TimelineFireContext.h"
#include "../../Type/Transform.h"
#include "../../../externals/nlohmann/json.hpp"

namespace AbsoluteEngine {

class GameObject;

// レール座標での出現位置。ahead は、既定でゲームの照準距離付近(SetSpawnAheadDistance)を入れる
struct RailSpawnPoint {
    float ahead = 0.0f;
    float lateral = 0.0f;
    float vertical = 0.0f;
};

// JSON の「rail」項目への変換。項目名は "ahead" / "lateral" / "vertical"。足りない項目は 0
nlohmann::json RailSpawnPointToJson(const RailSpawnPoint& rail);
RailSpawnPoint RailSpawnPointFromJson(const nlohmann::json& j);

// レール座標を世界座標へ変換して、inOutTransform の translate に書き込む(rotate / scale は触らない)。
// outFrame には、出現位置(ahead 地点)のレールの座標系を返す。敵の向きを決めたり、編隊を散らす向きに使う。
// タイムラインのレール(context.camera)が無いときは、何も書き換えずに false を返す(呼び出し側は、元の世界座標を使う)
bool ResolveRailTransform(const RailSpawnPoint& rail, const TimelineFireContext& context,
                          Transform& inOutTransform, RailFrame& outFrame);

// 出現したオブジェクトの全コンポーネントへ、「レール上に出現した」ことと、その位置の座標系を知らせる
// (IComponent::OnSpawnedOnRail)。進行方向に依存するコンポーネントが、自分の向きを決めるために使う
void NotifySpawnedOnRail(GameObject& object, const RailFrame& frame);

// エディタ(インスペクタ)用: レール座標を使うかどうかのチェックと、ahead / lateral / vertical の入力欄を描く。
// USE_IMGUI が無いビルドでは何もしない。activated / deactivatedAfterEdit は、Undo 用の判定の出力引数
void DrawRailSpawnPointUI(bool& useRail, RailSpawnPoint& rail, bool& activated, bool& deactivatedAfterEdit);

} // namespace AbsoluteEngine
