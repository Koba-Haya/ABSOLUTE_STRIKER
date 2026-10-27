#include "RebirthDirector.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kTwoPi = 6.2831853f;
constexpr float kMinSeconds = 0.001f;

// 0〜1 の値を、始まりと終わりがなめらかな曲線にする
float Smooth(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t) { return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t)}; }

// 長さが 0 の段階で 0 割りにならないよう、段階の進み(0〜1)を求める
float Progress(float elapsed, float start, float length) {
  return length > 0.0f ? std::clamp((elapsed - start) / length, 0.0f, 1.0f) : (elapsed >= start ? 1.0f : 0.0f);
}

#ifdef USE_IMGUI
constexpr float kDebugWindowX = 420.0f;
constexpr float kDebugWindowY = 10.0f;
constexpr float kSecondsMax = 6.0f;
constexpr float kBoostMax = 4.0f;
constexpr float kScaleMax = 4.0f;
constexpr float kDistanceMax = 40.0f;
constexpr float kFlyDistanceMax = 80.0f;
constexpr float kSpeedScaleMax = 10.0f;
constexpr float kBurstCountMax = 700.0f;
constexpr float kBurstSpeedMax = 40.0f;
#endif
} // namespace

void RebirthDirector::Begin(const RebirthStart &start) {
  if (IsActive()) return;
  start_ = start;
  elapsed_ = 0.0f;
  sceneChangeRequested_ = false;
  sceneChangePending_ = false;
  burstTriggered_ = false;
  burstPending_ = false;
  Evaluate_();
}

float RebirthDirector::GetTotalSeconds() const {
  return settings_.gatherSeconds + settings_.appearSeconds + settings_.chargeSeconds + settings_.burstSeconds + settings_.settleSeconds +
         settings_.flySeconds;
}

void RebirthDirector::Update(float deltaTime) {
  if (!IsActive()) return;
  elapsed_ += deltaTime;
  Evaluate_();
}

bool RebirthDirector::ConsumeSceneChange() {
  const bool pending = sceneChangePending_;
  sceneChangePending_ = false;
  return pending;
}

bool RebirthDirector::ConsumeBurst() {
  const bool pending = burstPending_;
  burstPending_ = false;
  return pending;
}

void RebirthDirector::Evaluate_() {
  const RebirthSettings &s = settings_;
  const float gatherEnd = s.gatherSeconds;
  const float appearEnd = gatherEnd + s.appearSeconds;
  const float chargeEnd = appearEnd + s.chargeSeconds;
  const float burstEnd = chargeEnd + s.burstSeconds;
  const float settleEnd = burstEnd + s.settleSeconds;
  const float flyEnd = settleEnd + s.flySeconds;

  RebirthState st;
  // 段階の判定
  if (elapsed_ >= flyEnd) st.phase = RebirthPhase::Done;
  else if (elapsed_ >= settleEnd) st.phase = RebirthPhase::Fly;
  else if (elapsed_ >= burstEnd) st.phase = RebirthPhase::Settle;
  else if (elapsed_ >= chargeEnd) st.phase = RebirthPhase::Burst;
  else if (elapsed_ >= appearEnd) st.phase = RebirthPhase::Charge;
  else if (elapsed_ >= gatherEnd) st.phase = RebirthPhase::Appear;
  else st.phase = RebirthPhase::Gather;
  const bool flying = st.phase == RebirthPhase::Fly || st.phase == RebirthPhase::Done;

  const float gatherP = Progress(elapsed_, 0.0f, s.gatherSeconds);
  const float appearP = Progress(elapsed_, gatherEnd, s.appearSeconds);
  const float chargeP = Progress(elapsed_, appearEnd, s.chargeSeconds);
  const float burstP = Progress(elapsed_, chargeEnd, s.burstSeconds);
  const float settleP = Progress(elapsed_, burstEnd, s.settleSeconds);
  const float flyP = Progress(elapsed_, settleEnd, s.flySeconds);
  const float sinceBurst = elapsed_ - chargeEnd; // バーストが始まってからの秒(始まる前は負)

  // --- 灰と核 ---
  st.gatherSpeedScale = st.phase == RebirthPhase::Gather ? s.gatherSpeedScale : 1.0f;
  switch (st.phase) {
  case RebirthPhase::Gather:
    st.coreBoost = s.boostPeak * Smooth(gatherP); // 核が強く光る
    break;
  case RebirthPhase::Appear:
    // 蝶が現れ始めたら素早く弱め(光に覆われないように)、残りの間は保つ
    st.coreBoost = Lerp(s.boostPeak, s.boostAfterAppear, Smooth(appearP / (std::max)(s.coreFadeRatio, kMinSeconds)));
    break;
  case RebirthPhase::Charge: {
    // 溜め: 核が小さく強くなり(圧縮)、灰が胴体へ強く吸い込まれる。蝶の震えに合わせて脈打ち、最後の chargeStillSeconds は静止する
    // 静止の区間は、アニメの静止(reviveStillStartSec から reviveChargeEndSec)に当たる、溜めの最後の割合(例: 1.8〜2.0 は、溜めの最後の 20%)
    const float chargeAnimLength = (std::max)(s.reviveChargeEndSec - s.reviveAppearEndSec, kMinSeconds);
    const float stillRatio = std::clamp((s.reviveChargeEndSec - s.reviveStillStartSec) / chargeAnimLength, 0.0f, 1.0f);
    const float stillSeconds = s.chargeSeconds * stillRatio;
    const float still = 1.0f - Smooth(Progress(elapsed_, chargeEnd - stillSeconds, stillSeconds)); // 静止の間は 0
    const float pulse = 1.0f + s.chargePulseDepth * still * std::sin(elapsed_ * kTwoPi * s.chargePulseHz);
    st.coreBoost = Lerp(s.boostAfterAppear, s.chargeBoost, Smooth(chargeP)) * pulse;
    st.coreSizeScale = Lerp(1.0f, s.chargeCoreSizeScale, Smooth(chargeP));
    st.convergeScale = Lerp(1.0f, s.chargeConvergeScale, Smooth(chargeP));
    break;
  }
  case RebirthPhase::Burst:
    // バースト: 溜めた光を使い切って消える。圧縮された大きさと収束の強さも、元へ戻す
    st.coreBoost = Lerp(s.chargeBoost, s.boostExtinguished, Smooth(burstP * 2.0f));
    st.coreSizeScale = Lerp(s.chargeCoreSizeScale, 1.0f, Smooth(burstP));
    st.convergeScale = Lerp(s.chargeConvergeScale, 1.0f, Smooth(burstP));
    break;
  default:
    st.coreBoost = s.boostExtinguished;
    break;
  }

  // 核の追従: Appear の始まりから coreMoveSeconds で、巣から胴体へ移る。灰の発生は、Charge の間に絞って止める
  st.coreFollowBlend = st.phase == RebirthPhase::Gather ? 0.0f : Smooth(Progress(elapsed_, gatherEnd, s.coreMoveSeconds));
  st.ashSpawnScale = 1.0f - Smooth(Progress(elapsed_, appearEnd, s.chargeSeconds));

  // --- 見出しと案内: Gather の前半で消える ---
  st.uiOpacity = 1.0f - Smooth(Progress(elapsed_, 0.0f, s.gatherSeconds * s.uiFadeRatio));

  // --- アニメ revive の時刻: Appear = 現れ、Charge = 溜め、Burst = バースト、Settle = 落ち着き。各段階の進みで、区間の中を進める ---
  switch (st.phase) {
  case RebirthPhase::Appear:
    st.reviveActive = true;
    st.reviveSeconds = Lerp(0.0f, s.reviveAppearEndSec, appearP);
    break;
  case RebirthPhase::Charge:
    st.reviveActive = true;
    st.reviveSeconds = Lerp(s.reviveAppearEndSec, s.reviveChargeEndSec, chargeP);
    break;
  case RebirthPhase::Burst:
    st.reviveActive = true;
    st.reviveSeconds = Lerp(s.reviveChargeEndSec, s.reviveBurstEndSec, burstP);
    break;
  case RebirthPhase::Settle:
    st.reviveActive = true;
    st.reviveSeconds = Lerp(s.reviveBurstEndSec, s.reviveLengthSec, settleP);
    break;
  default:
    break;
  }
  st.reviveLengthSeconds = s.reviveLengthSec;

  // --- 世界の時間(ヒットストップ)と画角(寄り): バーストの始まりから ---
  if (st.phase == RebirthPhase::Burst && sinceBurst < s.hitStopSeconds) {
    st.timeScale = s.hitStopScale;
  }
  if (sinceBurst >= 0.0f) {
    // 寄り: zoomInSeconds で画角を狭め、zoomOutSeconds で戻す(どちらも ease)
    const float in = Smooth(Progress(sinceBurst, 0.0f, s.zoomInSeconds));
    const float out = Smooth(Progress(sinceBurst, s.zoomInSeconds, s.zoomOutSeconds));
    st.fovScale = 1.0f - s.zoomAmount * in * (1.0f - out);
  }

  // --- 蝶 ---
  const Vector3 core = start_.corePosition;
  const Vector3 hoverPosition{core.x, core.y + s.hoverHeight, core.z};
  st.butterflyVisible = st.phase != RebirthPhase::Gather;
  st.butterflyRotate = {0.0f, 0.0f, 0.0f};
  st.butterflyScale = s.startScale;
  Vector3 basePosition{core.x, core.y + s.appearLift, core.z};
  switch (st.phase) {
  case RebirthPhase::Appear: {
    // ディゾルブの逆再生: しきい値を 1(何も見えない)から 0(全部見える)へ。胴体が先に形になり(bodyThreshold まで)、
    // 翅は遅れて(wingDelayRatio のあと)、外側ほど遅れて現れる。縁は熾火で光る
    st.dissolveEnabled = true;
    const float delay = std::clamp(s.wingDelayRatio, 0.01f, 0.99f);
    st.dissolveThreshold = appearP < delay ? Lerp(1.0f, s.bodyThreshold, Smooth(appearP / delay))
                                           : Lerp(s.bodyThreshold, 0.0f, Smooth((appearP - delay) / (1.0f - delay)));
    st.dissolveEdgeRange = s.dissolveEdgeRange;
    st.butterflyScale = Lerp(s.startScale, s.appearEndScale, Smooth(appearP));
    break;
  }
  case RebirthPhase::Charge:
    st.butterflyScale = s.appearEndScale; // 溜め: 大きさも位置も保つ(アニメが体を沈め、震わせる)
    break;
  case RebirthPhase::Burst:
    // バースト: 一気に広がりきる大きさと、浮かぶ高さへ
    st.butterflyScale = Lerp(s.appearEndScale, s.fullScale, Smooth(burstP));
    basePosition = {core.x, core.y + Lerp(s.appearLift, s.hoverHeight, Smooth(burstP)), core.z};
    break;
  case RebirthPhase::Settle:
    // 落ち着き: 少し浮かぶ
    st.butterflyScale = s.fullScale;
    basePosition = {core.x, core.y + s.hoverHeight + s.settleRise * Smooth(settleP), core.z};
    break;
  case RebirthPhase::Fly:
  case RebirthPhase::Done: {
    // 飛び立ち: 加速しながら、上と前方(+z)へ。機首が上がる
    st.flapping = true;
    st.butterflyScale = s.fullScale;
    const float accel = std::pow(flyP, s.flyEasePower);
    const Vector3 settledPosition{hoverPosition.x, hoverPosition.y + s.settleRise, hoverPosition.z};
    basePosition = {settledPosition.x, settledPosition.y + s.flyUp * accel, settledPosition.z + s.flyForward * accel};
    st.butterflyRotate = {-s.noseUpRadians * Smooth(flyP), 0.0f, 0.0f};
    break;
  }
  default:
    break;
  }
  st.butterflyPosition = basePosition;

  // --- 翅の閃光: バーストの瞬間に最大で、すぐ戻る ---
  // 翅の閃光: 開きすぎの時刻(バーストの始まりから overshootDelay 後)にピークで、そこから flashSeconds で戻る。ピークまでは素早く立ち上がる
  const float burstAnimLength = (std::max)(s.reviveBurstEndSec - s.reviveChargeEndSec, kMinSeconds);
  const float overshootDelay = s.burstSeconds * std::clamp((s.reviveOvershootSec - s.reviveChargeEndSec) / burstAnimLength, 0.0f, 1.0f);
  if (sinceBurst >= 0.0f) {
    const float rise = Smooth(Progress(sinceBurst, 0.0f, overshootDelay));
    const float fall = 1.0f - Smooth(Progress(sinceBurst, overshootDelay, s.flashSeconds));
    st.flashAmount = (sinceBurst < overshootDelay ? rise : fall) * s.flashGain;
    st.flashColor = s.flashColor;
  }

  // --- バーストが始まる合図(1 回) ---
  if (!burstTriggered_ && sinceBurst >= overshootDelay) {
    burstTriggered_ = true;
    burstPending_ = true;
  }

  // --- カメラ: Appear から Charge にかけて、巣の近くへ寄りながら、注視点を巣から蝶へ移す(それ以後は動かさない)。Fly で蝶を追って上を向く ---
  st.cameraOverride = st.phase != RebirthPhase::Gather;
  const float pull = Smooth(Progress(elapsed_, gatherEnd, s.appearSeconds + s.chargeSeconds));
  // 降下の終点から、巣の近く(巣の中心の上 approachHeight、後ろ approachBack)へ寄る。蝶の誕生を近くで見せる
  const Vector3 approachEye{start_.nestCenter.x, start_.nestCenter.y + s.approachHeight, start_.nestCenter.z - s.approachBack};
  const Vector3 pulledEye = Lerp(start_.cameraEye, approachEye, pull);
  // 注視点は、揺れや浮き沈みを含まない位置(カメラが細かく動かないように)
  const Vector3 steadyTarget{core.x, core.y + (flying ? s.hoverHeight + s.settleRise : s.appearLift), core.z};
  st.cameraTarget = Lerp(start_.nestCenter, steadyTarget, pull);
  st.cameraEye = pulledEye;
  if (flying) {
    // 蝶の動きの一部だけ、カメラが追う(蝶が上へ昇るぶん、カメラは見上げる)
    const Vector3 moved{basePosition.x - steadyTarget.x, basePosition.y - steadyTarget.y, basePosition.z - steadyTarget.z};
    st.cameraEye = {pulledEye.x + moved.x * s.followRatio, pulledEye.y + moved.y * s.followRatio, pulledEye.z + moved.z * s.followRatio};
    st.cameraTarget = basePosition;
  }

  // --- 雲が画面を満たす ---
  if (flying) {
    st.cloudFill = Smooth(Progress(flyP, s.fillStart, s.fillEnd - s.fillStart));
  }

  // --- ゲームへの切り替えの要求(1 回) ---
  if (!sceneChangeRequested_ && elapsed_ >= settleEnd + s.flySeconds * s.switchRatio) {
    sceneChangeRequested_ = true;
    sceneChangePending_ = true;
  }

  state_ = st;
}

#ifdef USE_IMGUI
bool RebirthDirector::DrawDebugUi() {
  ImGui::SetNextWindowPos(ImVec2(kDebugWindowX, kDebugWindowY), ImGuiCond_FirstUseEver);
  ImGui::Begin("Rebirth");
  const bool start = ImGui::Button("Start rebirth (same as Retry)");
  static const char *kPhaseNames[] = {"Idle", "Gather", "Appear", "Charge", "Burst", "Settle", "Fly", "Done"};
  ImGui::Text("Phase: %s   elapsed %.2f / %.2f s   revive time %.2f", kPhaseNames[static_cast<int>(state_.phase)], elapsed_,
              GetTotalSeconds(), state_.reviveSeconds);

  RebirthSettings &s = settings_;
  ImGui::SeparatorText("Timeline (seconds)");
  ImGui::SliderFloat("Gather", &s.gatherSeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Appear", &s.appearSeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Charge", &s.chargeSeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Burst", &s.burstSeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Settle", &s.settleSeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Fly", &s.flySeconds, 0.1f, kSecondsMax, "%.2f");
  ImGui::SliderFloat("Scene change at (Fly ratio)", &s.switchRatio, 0.0f, 1.0f, "%.2f");
  ImGui::SeparatorText("Animation revive: section boundaries (seconds inside the animation)");
  ImGui::SliderFloat("Appear ends", &s.reviveAppearEndSec, 0.0f, 6.0f, "%.2f");
  ImGui::SliderFloat("Charge ends", &s.reviveChargeEndSec, 0.0f, 6.0f, "%.2f");
  ImGui::SliderFloat("Burst ends", &s.reviveBurstEndSec, 0.0f, 6.0f, "%.2f");
  ImGui::SliderFloat("Animation length", &s.reviveLengthSec, 0.5f, 6.0f, "%.2f");
  ImGui::SliderFloat("Still starts (Charge end)", &s.reviveStillStartSec, 0.0f, 6.0f, "%.2f");
  ImGui::SliderFloat("Overshoot (flash peak, particles)", &s.reviveOvershootSec, 0.0f, 6.0f, "%.2f");
  ImGui::SeparatorText("Gather / core");
  ImGui::SliderFloat("Gather speed scale", &s.gatherSpeedScale, 1.0f, kSpeedScaleMax, "%.1f");
  ImGui::SliderFloat("Core boost peak", &s.boostPeak, 0.0f, kBoostMax, "%.2f");
  ImGui::SliderFloat("Core boost after appear", &s.boostAfterAppear, -1.0f, kBoostMax, "%.2f");
  ImGui::SliderFloat("Core fade (Appear ratio)", &s.coreFadeRatio, 0.05f, 1.0f, "%.2f");
  ImGui::SliderFloat("Core move to body (s)", &s.coreMoveSeconds, 0.05f, 2.0f, "%.2f");
  ImGui::SliderFloat("UI fade (Gather ratio)", &s.uiFadeRatio, 0.05f, 1.0f, "%.2f");
  ImGui::SeparatorText("Charge");
  ImGui::SliderFloat("Charge core boost", &s.chargeBoost, -1.0f, kBoostMax, "%.2f");
  ImGui::SliderFloat("Charge core size scale", &s.chargeCoreSizeScale, 0.05f, 1.0f, "%.2f");
  ImGui::SliderFloat("Charge converge scale", &s.chargeConvergeScale, 1.0f, 8.0f, "%.1f");
  ImGui::SliderFloat("Charge pulse Hz", &s.chargePulseHz, 0.0f, 20.0f, "%.1f");
  ImGui::SliderFloat("Charge pulse depth", &s.chargePulseDepth, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Core boost extinguished", &s.boostExtinguished, -2.0f, 0.0f, "%.2f");
  ImGui::SeparatorText("Wings appear (dissolve)");
  ImGui::SliderFloat("Wing delay (Appear ratio)", &s.wingDelayRatio, 0.01f, 0.99f, "%.2f");
  ImGui::SliderFloat("Body threshold (just above the wings, 0.8)", &s.bodyThreshold, 0.5f, 1.0f, "%.2f");
  ImGui::SliderFloat("Dissolve edge range", &s.dissolveEdgeRange, 0.0f, 0.5f, "%.2f");
  ImGui::SeparatorText("Butterfly");
  ImGui::SliderFloat("Start scale", &s.startScale, 0.05f, kScaleMax, "%.2f");
  ImGui::SliderFloat("Appear end scale", &s.appearEndScale, 0.05f, kScaleMax, "%.2f");
  ImGui::SliderFloat("Full scale (after the burst)", &s.fullScale, 0.05f, kScaleMax, "%.2f");
  ImGui::SliderFloat("Appear lift", &s.appearLift, 0.0f, kDistanceMax, "%.1f");
  ImGui::SliderFloat("Hover height", &s.hoverHeight, 0.0f, kDistanceMax, "%.1f");
  ImGui::SliderFloat("Settle rise", &s.settleRise, 0.0f, 5.0f, "%.2f");
  ImGui::SeparatorText("Burst");
  ImGui::SliderFloat("Hit stop seconds", &s.hitStopSeconds, 0.0f, 0.5f, "%.2f");
  ImGui::SliderFloat("Hit stop time scale", &s.hitStopScale, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Zoom amount (fov)", &s.zoomAmount, 0.0f, 0.2f, "%.3f");
  ImGui::SliderFloat("Zoom in seconds", &s.zoomInSeconds, 0.01f, 1.0f, "%.2f");
  ImGui::SliderFloat("Zoom out seconds", &s.zoomOutSeconds, 0.01f, 1.0f, "%.2f");
  ImGui::SliderFloat("Flash seconds", &s.flashSeconds, 0.05f, 1.0f, "%.2f");
  ImGui::SliderFloat("Flash gain", &s.flashGain, 0.0f, 8.0f, "%.2f");
  ImGui::ColorEdit3("Flash color", &s.flashColor.x);
  ImGui::SliderFloat("Burst count", &s.burstCount, 0.0f, kBurstCountMax, "%.0f");
  ImGui::SliderFloat("Burst radius", &s.burstRadius, 0.0f, 3.0f, "%.2f");
  ImGui::SliderFloat("Burst speed min", &s.burstSpeedMin, 0.0f, kBurstSpeedMax, "%.1f");
  ImGui::SliderFloat("Burst speed max", &s.burstSpeedMax, 0.0f, kBurstSpeedMax, "%.1f");
  ImGui::SliderFloat("Burst upward", &s.burstUpward, 0.0f, 20.0f, "%.1f");
  ImGui::SliderFloat("Burst life min", &s.burstLifeMin, 0.2f, 8.0f, "%.2f");
  ImGui::SliderFloat("Burst life max", &s.burstLifeMax, 0.2f, 8.0f, "%.2f");
  ImGui::SliderFloat("Burst size min", &s.burstSizeMin, 0.01f, 0.5f, "%.2f");
  ImGui::SliderFloat("Burst size max", &s.burstSizeMax, 0.01f, 0.5f, "%.2f");
  ImGui::ColorEdit3("Burst color (start, gold)", &s.burstGold.x);
  ImGui::ColorEdit3("Burst color (end, blue)", &s.burstTeal.x);
  ImGui::SliderFloat("Ground count", &s.groundCount, 0.0f, 400.0f, "%.0f");
  ImGui::SliderFloat("Ground speed min", &s.groundSpeedMin, 0.0f, kBurstSpeedMax, "%.1f");
  ImGui::SliderFloat("Ground speed max", &s.groundSpeedMax, 0.0f, kBurstSpeedMax, "%.1f");
  ImGui::SliderFloat("Ground upward", &s.groundUpward, 0.0f, 10.0f, "%.1f");
  ImGui::SeparatorText("Fly / camera / cloud");
  ImGui::SliderFloat("Fly up", &s.flyUp, 0.0f, kFlyDistanceMax, "%.1f");
  ImGui::SliderFloat("Fly forward (+z)", &s.flyForward, 0.0f, kFlyDistanceMax, "%.1f");
  ImGui::SliderFloat("Fly ease power", &s.flyEasePower, 1.0f, 4.0f, "%.2f");
  ImGui::SliderFloat("Nose up (rad)", &s.noseUpRadians, 0.0f, 1.5f, "%.2f");
  ImGui::SliderFloat("Approach height (above nest)", &s.approachHeight, 1.0f, kDistanceMax, "%.1f");
  ImGui::SliderFloat("Approach back", &s.approachBack, 1.0f, kDistanceMax, "%.1f");
  ImGui::SliderFloat("Follow ratio (Fly)", &s.followRatio, 0.0f, 1.5f, "%.2f");
  ImGui::SliderFloat("Cloud fill start (Fly ratio)", &s.fillStart, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Cloud fill end (Fly ratio)", &s.fillEnd, 0.0f, 1.0f, "%.2f");
  ImGui::End();
  return start;
}
#endif
