#pragma once

class Renderer;

namespace AbsoluteEngine {

class SceneWorld;

// シーン内のライト(LightNodeComponent)を集めて、描画側(Renderer)へ渡すクラス。
//
// 分けた理由: SceneWorld(オブジェクトの管理)が描画側の Renderer を知ると依存が逆向きになるため、
// 「世界を読んで描画へ橋渡しする」役割を別クラスにした。状態を持たないので、メソッドは static。
class SceneLights {
public:
  // world 内の全オブジェクト(子も含む)からライトを集め、Renderer の平行光源・点光源・スポットライトへ設定する。
  // renderer が nullptr のときは何もしない
  static void ApplyToRenderer(const SceneWorld& world, Renderer* renderer);
};

} // namespace AbsoluteEngine
