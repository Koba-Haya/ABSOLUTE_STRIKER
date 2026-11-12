// ============================================================
// TimelineFireContext.h
// 役割: タイムラインのイベントが発火(Fire)するときに、トラックから渡される情報。
//       イベントは「いつ発火するか」を知らない作りなので(時刻はトラックが持つ)、
//       発火時刻と、その時刻のレール上の距離は、この文脈を通して受け取る。
//
// なぜ別ファイルか: ITimelineEvent(イベントの共通の窓口)と RailSpawnPoint(レール座標の出現位置)の
//       両方が使うので、どちらにも属さない小さな型として切り出し、循環した include を避ける。
// ============================================================
#pragma once
#include "../../camera/ITimelineCamera.h"

namespace AbsoluteEngine {

struct TimelineFireContext {
    // このイベントの発火時刻(秒)。実際に Fire() が呼ばれた時刻ではなく、イベントに設定された時刻。
    // フレームの区切りで発火が遅れても、出現位置がぶれないようにするため、設定された時刻のほうを使う
    float triggerTime = 0.0f;

    // タイムラインに結びついたレール(カメラ)。無い場合は nullptr
    const ITimelineCamera* camera = nullptr;

    // 発火時刻における、レール上の距離 s(弧長、ユニット)を返す。
    // タイムラインの時間とレールの進行度は 1 対 1(進行度 = 時刻 / Duration)なので、s = 進行度 x 全長。
    // カメラが無い、またはレールの長さが 0 のときは 0 を返す
    float GetRailDistance() const {
        if (camera == nullptr) {
            return 0.0f;
        }
        const float duration = camera->GetDuration();
        if (duration <= 0.0f) {
            return 0.0f;
        }
        return (triggerTime / duration) * camera->GetTotalLength();
    }
};

} // namespace AbsoluteEngine
