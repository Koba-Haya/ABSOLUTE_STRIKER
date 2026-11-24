#pragma once
#include "Component.h"
#include "ComponentDeps.h"
#include <string>
#include <map>
#include <functional>
#include <iostream>
#include <memory>

// 基本コンポーネントのインクルード
#include "ModelComponent.h"
#include "ColliderComponent.h"
#include "LightNodeComponent.h"
#include "RigidbodyComponent.h"

namespace AbsoluteEngine {

// 部品の名前から、部品を作るクラス(JSON の読み込みや、エディタの「部品を追加」で使う)。
// 作る側が、窓口の束(ComponentDeps)を渡す。窓口を要する部品は、束から取り出して、自分のコンストラクタへ渡す。
class ComponentFactory {
public:
    // 窓口を要さない部品の作り方(引数なし)
    using CreatorFunc = std::function<std::unique_ptr<IComponent>()>;
    // 窓口を要する部品の作り方(窓口の束を受け取る)
    using DepsCreatorFunc = std::function<std::unique_ptr<IComponent>(const ComponentDeps&)>;

    static ComponentFactory& GetInstance() {
        static ComponentFactory instance;
        return instance;
    }

    // 窓口を要さない部品を登録する。内部では、束を無視する形に包んで、1種類の作り方にそろえて持つ
    void Register(const std::string& name, CreatorFunc func) {
        creators_[name] = [func = std::move(func)](const ComponentDeps&) { return func(); };
    }

    // 窓口を要する部品を登録する(束から窓口を取り出して、部品のコンストラクタへ渡す関数を渡す)
    void Register(const std::string& name, DepsCreatorFunc func) {
        creators_[name] = std::move(func);
    }

    // 名前の部品を作る。名前が未登録なら nullptr。deps: 部品へ渡す窓口の束(作る側が設定したもの)
    std::unique_ptr<IComponent> Create(const std::string& name, const ComponentDeps& deps) {
        auto it = creators_.find(name);
        if (it != creators_.end()) {
            return it->second(deps);
        }
        return nullptr;
    }

    // 窓口(ISpawner、IScreenView など)を要する部品を登録する。
    // maker: 窓口がそろった束を受け取り、部品を作って返す関数(束から、その部品が要る窓口だけを取り出して、コンストラクタへ渡す)。
    // 束に窓口がそろっていないときは、maker を呼ばず、部品を作らずに(nullptr)、エラーを出す
    template <typename Maker>
    void RegisterWithServices(const std::string& name, Maker maker) {
        Register(name, DepsCreatorFunc([name, maker](const ComponentDeps& deps) -> std::unique_ptr<IComponent> {
            if (!deps.IsComplete()) {
                std::cerr << "[ComponentFactory] 窓口がそろっていないため、部品を作れません: " << name << std::endl;
                return nullptr;
            }
            return maker(deps);
        }));
    }

    std::vector<std::string> GetRegisteredComponentNames() const {
        std::vector<std::string> names;
        for (const auto& pair : creators_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    ComponentFactory() {
        // デフォルトの基本コンポーネントを登録
        Register("ModelComponent", []() { return std::make_unique<ModelComponent>(); });
        Register("ColliderComponent", []() { return std::make_unique<ColliderComponent>(); });
        Register("LightNodeComponent", []() { return std::make_unique<LightNodeComponent>(); });
        Register("RigidbodyComponent", []() { return std::make_unique<RigidbodyComponent>(); });
    }
    std::map<std::string, DepsCreatorFunc> creators_;
};

} // namespace AbsoluteEngine
