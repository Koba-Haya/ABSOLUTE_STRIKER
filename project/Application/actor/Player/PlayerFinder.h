#pragma once
#include <memory>

namespace AbsoluteEngine {
class GameObject;
class ISceneQuery;
} // namespace AbsoluteEngine

// 自機(PlayerComponent を持つオブジェクト)を探す処理を、1 か所にまとめたもの。
//
// 作った理由: 敵、ボス、アイテムの部品が、それぞれ同じ「全オブジェクトをたどって自機を探す」ループを持っていた(7 か所の複製)。
// また、エンジン側の窓口(ISceneQuery)は、ゲーム固有の型(PlayerComponent)を知らない作りなので、「自機を探す」は、ゲーム側のここに置く。
namespace PlayerFinder {

// query の一覧から、自機を探して返す。見つからなければ nullptr
std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const AbsoluteEngine::ISceneQuery& query);

} // namespace PlayerFinder
