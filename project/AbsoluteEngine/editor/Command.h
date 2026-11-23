#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include "../scene/GameObject.h"
#include "../scene/SceneWorld.h"
#include "../Type/Transform.h"
#include "../../externals/nlohmann/json.hpp"

namespace AbsoluteEngine {

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0; // Redo時または初回実行時
    virtual void Undo() = 0;    // Undo時
};

// Transform変更のコマンド
class TransformCommand : public ICommand {
public:
    TransformCommand(std::shared_ptr<GameObject> target, const Transform& before, const Transform& after)
        : target_(target), before_(before), after_(after) {}

    void Execute() override {
        if (auto t = target_.lock()) {
            t->SetTransform(after_);
        }
    }

    void Undo() override {
        if (auto t = target_.lock()) {
            t->SetTransform(before_);
        }
    }

private:
    std::weak_ptr<GameObject> target_;
    Transform before_;
    Transform after_;
};

// オブジェクト生成のコマンド
class CreateObjectCommand : public ICommand {
public:
    CreateObjectCommand(std::shared_ptr<GameObject> target, SceneWorld* world)
        : target_(target), world_(world) {}

    CreateObjectCommand(std::shared_ptr<GameObject> target, std::shared_ptr<GameObject> parent)
        : target_(target), parent_(parent), world_(nullptr) {}

    void Execute() override {
        if (auto p = parent_.lock()) {
            p->AddChild(target_);
        } else if (world_) {
            world_->Add(target_);
        }
    }

    void Undo() override {
        if (auto p = parent_.lock()) {
            p->RemoveChild(target_);
        } else if (world_) {
            world_->Remove(target_);
        }
    }

private:
    std::shared_ptr<GameObject> target_;
    std::weak_ptr<GameObject> parent_;
    SceneWorld* world_; // 所有しない。ルート直下のオブジェクトの追加・削除を、このシーンの世界へ依頼する
};

// オブジェクト削除のコマンド
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(std::shared_ptr<GameObject> target, SceneWorld* world)
        : target_(target), world_(world) {}

    DeleteObjectCommand(std::shared_ptr<GameObject> target, std::shared_ptr<GameObject> parent)
        : target_(target), parent_(parent), world_(nullptr) {}

    void Execute() override {
        if (auto p = parent_.lock()) {
            p->RemoveChild(target_);
        } else if (world_) {
            world_->Remove(target_);
        }
    }

    void Undo() override {
        if (auto p = parent_.lock()) {
            p->AddChild(target_);
        } else if (world_) {
            world_->Add(target_);
        }
    }

private:
    std::shared_ptr<GameObject> target_;
    std::weak_ptr<GameObject> parent_;
    SceneWorld* world_; // 所有しない。ルート直下のオブジェクトの追加・削除を、このシーンの世界へ依頼する
};

// 汎用コンポーネント状態変更のコマンド
class ComponentStateCommand : public ICommand {
public:
    ComponentStateCommand(std::shared_ptr<GameObject> target, const std::string& componentType, const nlohmann::json& beforeState, const nlohmann::json& afterState)
        : target_(target), componentType_(componentType), before_(beforeState), after_(afterState) {}

    void Execute() override {
        if (auto t = target_.lock()) {
            for (const auto& comp : t->GetComponents()) {
                if (comp->GetTypeName() == componentType_) {
                    comp->Deserialize(after_);
                    break;
                }
            }
        }
    }

    void Undo() override {
        if (auto t = target_.lock()) {
            for (const auto& comp : t->GetComponents()) {
                if (comp->GetTypeName() == componentType_) {
                    comp->Deserialize(before_);
                    break;
                }
            }
        }
    }

private:
    std::weak_ptr<GameObject> target_;
    std::string componentType_;
    nlohmann::json before_;
    nlohmann::json after_;
};

// -----------------------------------------------------------
// タイムライン操作のコマンド（タスクF: Undo/Redo）
// -----------------------------------------------------------
// TimelineManager の状態変化（イベント追加・削除・プロパティ変更など）を
// 変更前後の JSON スナップショット文字列で記録する軽量な Undo コマンド。
// undoAction_ / redoAction_ はラムダで実装側が注入するため、
// TimelineManager への直接依存をこのヘッダから排除できる。
// -----------------------------------------------------------
class TimelineCommand : public ICommand {
public:
    // コンストラクタ:
    //   undoAction = Undo時に実行する処理（通常 beforeJson を LoadFromString するラムダ）
    //   redoAction = Redo時に実行する処理（通常 afterJson  を LoadFromString するラムダ）
    TimelineCommand(std::function<void()> undoAction, std::function<void()> redoAction)
        : undoAction_(std::move(undoAction))
        , redoAction_(std::move(redoAction)) {}

    // Redo / 初回実行
    void Execute() override {
        if (redoAction_) redoAction_();
    }

    // Undo
    void Undo() override {
        if (undoAction_) undoAction_();
    }

private:
    std::function<void()> undoAction_; // Undo時の処理
    std::function<void()> redoAction_; // Redo時の処理
};

} // namespace AbsoluteEngine
