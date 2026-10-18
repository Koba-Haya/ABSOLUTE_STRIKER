#pragma once
#include "Vector.h"
#include <cstddef>
#include <deque>
#include <vector>

/// <summary>
/// リボンの頂点1つ分。GPU の頂点バッファへそのまま書き込める並び(位置・UV・色)。
/// </summary>
struct RibbonVertex {
    Vector3 position;
    Vector2 texcoord; // x: 点の年齢の割合(先端 0 から、寿命に達した尾 1)、y: 幅方向(左端 0 から右端 1)
    Vector4 color;
};

/// <summary>
/// リボン(帯状の軌跡)の元になる「通過点の履歴」を管理し、帯の頂点を生成するクラス。
/// 弾などの位置を毎フレーム受け取り、前の点から一定距離離れたときだけ新しい点を記録する(距離ベース)。
/// 点は時間とともに古くなり、寿命を超えたものから削除される。
/// GPU には依存しない(頂点を作るだけで、描画は RibbonDrawer と Renderer が行う)。
/// </summary>
class RibbonTrail {
public:
    /// <summary>
    /// 履歴の記録ルールと、帯の見た目。
    /// </summary>
    struct Params {
        // --- 履歴の記録ルール ---
        float minPointDistance = 0.3f; // 前の点からこの距離以上離れたら新しい点を記録する
        float lifetime = 0.5f;         // 点の寿命(秒)。超えた点は古い側から削除される
        std::size_t maxPoints = 64;    // 記録する点の最大数(バッファの上限)

        // --- 帯の見た目 ---
        float width = 0.4f;                         // 先端の帯の幅(ワールド単位)
        float tailWidthRatio = 0.0f;                // 尾の幅の割合(先端の幅に対して。0 で尖る、1 で先細りなし)
        Vector4 color = { 0.3f, 1.0f, 1.0f, 1.0f }; // 帯の色(頂点カラー)。芯の白さとフェードはシェーダが付ける
    };

    /// <summary>
    /// 履歴の中の1点。
    /// </summary>
    struct Point {
        Vector3 position; // ワールド座標
        float age;        // 記録されてからの経過時間(秒)
    };

public:
    RibbonTrail() = default;
    ~RibbonTrail() = default;

    /// <summary>
    /// 設定を保存し、履歴を空にする。
    /// </summary>
    /// <param name="params">記録ルールと見た目</param>
    void Initialize(const Params& params);

    /// <summary>
    /// 履歴を更新する。毎フレーム呼ぶ。
    /// 点の経過時間を進めて古い点を削除し、先頭位置が十分動いていれば新しい点を記録する。
    /// </summary>
    /// <param name="deltaTime">経過時間(秒)</param>
    /// <param name="headPosition">現在の先頭位置(弾の現在座標)</param>
    void Update(float deltaTime, const Vector3& headPosition);

    /// <summary>
    /// 履歴をすべて消す。
    /// </summary>
    void Clear();

    /// <summary>
    /// 先頭位置を最後の点として確定し、以後は新しい点を記録しない状態にする(Update は呼ばなくなる)。
    /// 弾が消えるときに呼び、尾だけを Fade で消していくために使う。
    /// </summary>
    void Freeze();

    /// <summary>
    /// 新しい点を記録せず、点を古くして寿命切れを削除するだけの更新。Freeze のあとに毎フレーム呼ぶ。
    /// </summary>
    /// <param name="deltaTime">経過時間(秒)</param>
    void Fade(float deltaTime);

    /// <summary>
    /// 帯を作れる状態か(帯には通過点が2つ以上必要)。false になったら、尾は消えきったとみなせる。
    /// </summary>
    bool HasRibbon() const;

    /// <summary>
    /// 履歴から、カメラに面を向けた帯(ビルボード)の頂点を生成する。
    /// 頂点は「左・右・左・右...」の順で、トライアングルストリップとして描ける並びで出力される。
    /// 点が2つ未満のときは何も出力しない。
    /// </summary>
    /// <param name="cameraPosition">カメラのワールド座標(帯の幅方向を決めるのに使う)</param>
    /// <param name="outVertices">出力先。呼ぶたびに中身は空にされる</param>
    void BuildVertices(const Vector3& cameraPosition, std::vector<RibbonVertex>& outVertices) const;

    /// <summary>
    /// 記録済みの点(古い順)。
    /// </summary>
    const std::deque<Point>& GetPoints() const { return points_; }

private:
    /// <summary>
    /// 全ての点を古くし、寿命を超えたものを古い側から削除する。
    /// </summary>
    void AgePoints_(float deltaTime);

private:
    Params params_;
    std::deque<Point> points_; // 記録済みの点。front が最も古く、back が最も新しい
    Vector3 headPosition_{ 0.0f, 0.0f, 0.0f };
    bool hasHead_ = true; // true の間、帯の先端に現在の先頭位置を足す。Freeze で false になる
};
