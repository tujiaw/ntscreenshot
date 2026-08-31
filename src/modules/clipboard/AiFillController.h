#pragma once

#include "LlmClient.h"
#include "UiaHelper.h"

#include <windows.h>

#include <QString>
#include <vector>

// Orchestrates AI Fill across three phases (mirrors wtl_clipboard):
//   Phase 1 (caller thread):  Prepare()  — validate, UIA enumerate, build JSON
//   Phase 2 (worker thread):  RequestLlmFill() — HTTP call to LLM
//   Phase 3 (caller thread):  ApplyResults() — write values to controls via UIA
class AiFillController {
public:
    explicit AiFillController(const LlmConfig& config);

    bool Prepare(HWND targetWindow, const QString& clipboardText);
    std::vector<FillResult> RequestLlmFill();
    bool ApplyResults(const std::vector<FillResult>& results);

    QString LastError() const { return lastError_; }
    int LastAppliedCount() const { return lastAppliedCount_; }
    int LastFailedCount() const { return lastFailedCount_; }

    const std::vector<ControlInfo>& Controls() const { return controls_; }
    const QString& ControlsJson() const { return controlsJson_; }

private:
    static QString BuildControlsJson(const std::vector<ControlInfo>& controls);

    UiaHelper uia_;
    LlmClient llm_;
    QString lastError_;
    int lastAppliedCount_ = 0;
    int lastFailedCount_ = 0;

    std::vector<ControlInfo> controls_;
    QString clipboardText_;
    QString controlsJson_;
};
