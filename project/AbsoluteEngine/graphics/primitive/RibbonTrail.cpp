#include "RibbonTrail.h"
#include "Method.h"
#include <algorithm>

namespace {
// これより短いベクトルは、向きを持たないものとして扱う
constexpr float kMinDirectionLength = 0.0001f;

// 幅方向を決められなかったとき(最初の点など)に使う仮の向き
constexpr Vector3 kFallbackSide = { 1.0f, 0.0f, 0.0f };

Vector3 Sub(const Vector3& a, const Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
} // namespace

void RibbonTrail::Initialize(const Params& params) {
    params_ = params;
    hasHead_ = true;
    Clear();
}

void RibbonTrail::AgePoints_(float deltaTime) {
    // 全ての点を古くする
    for (auto& point : points_) {
        point.age += deltaTime;
    }

    // 寿命を超えた点を、古い側(front)から削除する。
    // 点は記録順に並んでいるので、front が寿命内なら残りも寿命内
    while (!points_.empty() && points_.front().age > params_.lifetime) {
        points_.pop_front();
    }
}

void RibbonTrail::Update(float deltaTime, const Vector3& headPosition) {
    AgePoints_(deltaTime);

    headPosition_ = headPosition;

    // 前の点からの距離を調べ、十分離れていれば新しい点として記録する。
    // 点が1つもないとき(最初のフレームなど)は、必ず記録する
    bool shouldAdd = points_.empty();
    if (!shouldAdd) {
        shouldAdd = Length(Sub(headPosition, points_.back().position)) >= params_.minPointDistance;
    }
    if (shouldAdd) {
        points_.push_back({ headPosition, 0.0f });
    }

    // 点の数が上限を超えたら、古い点から捨てる
    while (points_.size() > params_.maxPoints) {
        points_.pop_front();
    }
}

void RibbonTrail::Clear() {
    points_.clear();
}

void RibbonTrail::Freeze() {
    if (!hasHead_) return;

    // 先頭位置を、最後の点(年齢 0)として確定する。
    // 直前の点とほぼ同じ位置なら、長さ 0 の区間ができてしまうので足さない
    if (points_.empty() || Length(Sub(headPosition_, points_.back().position)) > kMinDirectionLength) {
        points_.push_back({ headPosition_, 0.0f });
    }
    hasHead_ = false;
}

void RibbonTrail::Fade(float deltaTime) {
    AgePoints_(deltaTime);
}

bool RibbonTrail::HasRibbon() const {
    return points_.size() + (hasHead_ ? 1 : 0) >= 2;
}

void RibbonTrail::BuildVertices(const Vector3& cameraPosition, std::vector<RibbonVertex>& outVertices) const {
    outVertices.clear();

    // 帯の通る位置を、古い順に並べる。最後に現在の先頭位置を足して、帯の先端を弾に一致させる。
    // (記録済みの点だけだと、次の点が記録されるまで先端が弾に追いつかない)
    // 各点には「年齢の割合」(0 で生まれたて、1 で寿命)を持たせる。先細りとフェードは、位置の順番ではなく年齢で決める。
    // こうすると、弾が止まっても尾は時間とともに細く薄くなり、点が消える瞬間に急に変わらない
    struct PathPoint {
        Vector3 position;
        float ageRatio;
    };
    std::vector<PathPoint> path;
    path.reserve(points_.size() + 1);
    for (const auto& point : points_) {
        const float ratio = params_.lifetime > 0.0f ? point.age / params_.lifetime : 1.0f;
        path.push_back({ point.position, std::clamp(ratio, 0.0f, 1.0f) });
    }
    if (hasHead_) {
        path.push_back({ headPosition_, 0.0f }); // 先頭位置は常に最新(年齢 0)
    }

    const std::size_t count = path.size();
    if (count < 2) return;

    outVertices.reserve(count * 2);
    const float headHalfWidth = params_.width * 0.5f;
    Vector3 prevSide = kFallbackSide;

    for (std::size_t i = 0; i < count; ++i) {
        // 進行方向: 両隣の点を結ぶ(端の点は片側だけ)。折れ線の角が鋭くならず、滑らかにつながる
        const Vector3& before = path[i == 0 ? 0 : i - 1].position;
        const Vector3& after = path[i + 1 < count ? i + 1 : i].position;
        const Vector3 tangent = Sub(after, before);

        const Vector3& position = path[i].position;
        const float u = path[i].ageRatio;

        // カメラへの向き
        const Vector3 toCamera = Sub(position, cameraPosition);

        // 幅方向: 進行方向とカメラへの向きの両方に垂直なベクトル。
        // これにより、帯の面が常にカメラの方を向く(ビルボード)
        Vector3 side = Cross(tangent, toCamera);
        if (Length(side) < kMinDirectionLength) {
            // 進行方向とカメラの向きが一直線に近いと、外積がほぼ 0 になり向きが決まらない。
            // その場合は直前の点の向きを引き継ぐ
            side = prevSide;
        } else {
            side = Normalize(side);
        }
        prevSide = side;

        // 先細り: 年齢の割合 u に応じて、先端の幅から尾の幅へ線形に変える
        const float halfWidth = headHalfWidth * (1.0f + (params_.tailWidthRatio - 1.0f) * u);

        // UV: u は年齢の割合(先端 0、尾 1)。v は左端 0、右端 1。シェーダがこれでフェードと芯の光を作る
        const Vector3 left = { position.x - side.x * halfWidth, position.y - side.y * halfWidth, position.z - side.z * halfWidth };
        const Vector3 right = { position.x + side.x * halfWidth, position.y + side.y * halfWidth, position.z + side.z * halfWidth };

        outVertices.push_back({ left, { u, 0.0f }, params_.color });
        outVertices.push_back({ right, { u, 1.0f }, params_.color });
    }
}
