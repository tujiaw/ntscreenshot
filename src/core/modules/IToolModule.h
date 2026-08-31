#pragma once

#include <QString>

// Minimal lifecycle boundary for independently developed tools. Modules expose
// only identity and lifecycle here; UI integration belongs in explicit module
// contracts instead of growing this interface into a service locator.
class IToolModule {
public:
    virtual ~IToolModule() = default;

    virtual QString id() const = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
};
