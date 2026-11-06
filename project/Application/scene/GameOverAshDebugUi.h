#pragma once
#include "GameOverAsh.h"

// 灰の流れ(GameOverAsh)の調整用 ImGui。Debug / Development のみ(USE_IMGUI が無ければ何もしない。Release は ImGui なし)。
// 数、速さ、集まる時間、色、光の強さと大きさ、脈打ちの速さを動かして、見た目を決める。気に入った値は、
// GameOverAsh.h / EmberCore.h / ScaleDust.h の既定値へ写す。
namespace GameOverAshDebugUi {
void Draw(GameOverAsh &ash);
} // namespace GameOverAshDebugUi
