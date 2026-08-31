#include "AiFillController.h"
#include <QDebug>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>

AiFillController::AiFillController(const LlmConfig& config)
    : llm_(config) {}

bool AiFillController::Prepare(HWND targetWindow, const QString& clipboardText) {
    lastError_.clear();
    controls_.clear();
    controlsJson_.clear();
    clipboardText_ = clipboardText;

    if (!targetWindow || !::IsWindow(targetWindow)) {
        lastError_ = QStringLiteral("Invalid target window.");
        return false;
    }
    if (clipboardText.isEmpty()) {
        lastError_ = QStringLiteral("Clipboard text is empty.");
        return false;
    }

    if (!uia_.Initialize()) {
        lastError_ = uia_.LastError();
        return false;
    }

    qInfo().noquote() << QStringLiteral("[AiFill] Phase 1: Enumerating controls...");
    controls_ = uia_.EnumerateEditableControls(targetWindow);
    qInfo().noquote() << QStringLiteral("[AiFill] Found %1 editable controls").arg(controls_.size());

    if (controls_.empty()) {
        lastError_ = uia_.LastError().isEmpty()
            ? QStringLiteral("No editable controls found in the target window.")
            : uia_.LastError();
        return false;
    }

    controlsJson_ = BuildControlsJson(controls_);
    qInfo().noquote() << QStringLiteral("[AiFill] Controls JSON:\n") + controlsJson_;
    return true;
}

std::vector<FillResult> AiFillController::RequestLlmFill() {
    qInfo().noquote() << QStringLiteral("[AiFill] Phase 2: Calling LLM (background thread)...");
    auto results = llm_.RequestFill(clipboardText_, controlsJson_);
    if (!llm_.LastError().isEmpty()) {
        lastError_ = llm_.LastError();
        return {};
    }
    qInfo().noquote() << QStringLiteral("[AiFill] LLM returned %1 fill actions").arg(results.size());
    return results;
}

bool AiFillController::ApplyResults(const std::vector<FillResult>& results) {
    lastError_.clear();
    lastAppliedCount_ = 0;
    lastFailedCount_ = 0;

    if (results.empty()) {
        qInfo().noquote() << QStringLiteral("[AiFill] Phase 3: No results to apply.");
        return true;
    }

    qInfo().noquote() << QStringLiteral("[AiFill] Phase 3: Applying %1 fills...").arg(results.size());

    QMap<int, IUIAutomationElement*> elementMap;
    for (const auto& c : controls_) {
        elementMap[c.index] = c.element.Get();
    }

    int successCount = 0;
    int failCount = 0;
    for (const auto& fr : results) {
        auto it = elementMap.find(fr.controlIndex);
        if (it == elementMap.end() || !it.value()) {
            ++failCount;
            qInfo().noquote() << QStringLiteral("  -> FAILED: control not found");
            continue;
        }
        if (uia_.FillControlValue(it.value(), fr.value)) {
            ++successCount;
            qInfo().noquote() << QStringLiteral("  -> OK");
        } else {
            ++failCount;
            qInfo().noquote() << QStringLiteral("  -> FAILED: write error");
        }
    }

    qInfo().noquote() << QStringLiteral("[AiFill] Done. Success: %1, Failed: %2")
                             .arg(successCount).arg(failCount);

    lastAppliedCount_ = successCount;
    lastFailedCount_ = failCount;

    if (failCount > 0 && successCount == 0) {
        lastError_ = QStringLiteral("All fills failed. Target app may not support UIA write operations.");
        return false;
    }
    if (failCount > 0) {
        lastError_ = QStringLiteral("Partial failure. Succeeded: %1, Failed: %2")
                         .arg(successCount).arg(failCount);
    }
    return successCount > 0;
}

QString AiFillController::BuildControlsJson(const std::vector<ControlInfo>& controls) {
    QJsonArray arr;
    for (const auto& c : controls) {
        QJsonObject obj;
        obj[QLatin1String("index")] = c.index;
        obj[QLatin1String("type")] = c.controlType;
        obj[QLatin1String("name")] = c.name;
        obj[QLatin1String("automation_id")] = c.automationId;
        obj[QLatin1String("current_value")] = c.currentValue;
        QJsonObject rect;
        rect[QLatin1String("left")] = static_cast<int>(c.boundingRect.left);
        rect[QLatin1String("top")] = static_cast<int>(c.boundingRect.top);
        rect[QLatin1String("width")] = static_cast<int>(c.boundingRect.right - c.boundingRect.left);
        rect[QLatin1String("height")] = static_cast<int>(c.boundingRect.bottom - c.boundingRect.top);
        obj[QLatin1String("rect")] = rect;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
