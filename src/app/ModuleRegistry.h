#pragma once

#include "core/modules/IToolModule.h"

#include <QStringList>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

class ModuleRegistry final {
public:
    ~ModuleRegistry();

    bool registerModule(std::unique_ptr<IToolModule> module);
    IToolModule* module(const QString& id) const;
    QStringList moduleIds() const;
    bool isInitialized() const { return initialized_; }

    template <typename T, typename... Args>
    T* emplaceModule(Args&&... args)
    {
        static_assert(std::is_base_of_v<IToolModule, T>);
        auto module = std::make_unique<T>(std::forward<Args>(args)...);
        T* result = module.get();
        return registerModule(std::move(module)) ? result : nullptr;
    }

    template <typename T>
    T* module() const
    {
        static_assert(std::is_base_of_v<IToolModule, T>);
        for (const auto& entry : modules_) {
            if (auto* result = dynamic_cast<T*>(entry.get())) {
                return result;
            }
        }
        return nullptr;
    }

    void initializeAll();
    void shutdownAll();

private:
    std::vector<std::unique_ptr<IToolModule>> modules_;
    std::size_t initializedCount_ = 0;
    bool initialized_ = false;
};
