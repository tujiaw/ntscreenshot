#include "modules/assistant/runtime/agent/AgentToolRegistry.h"
#include "modules/assistant/runtime/tools/LlmToolUtils.h"

#include <QJsonObject>
#include <cstdio>

namespace {
class LargeResultTool final : public LlmTools::LlmTool {
public:
    QString name() const override { return QStringLiteral("large_result"); }
    QString description() const override { return QStringLiteral("large result contract"); }
    QJsonObject parameters() const override { return {}; }
    QString execute(const QJsonObject &) const override {
        return QString(LlmTools::kToolOutputMaxChars + 5000, QLatin1Char('x'))
            + QStringLiteral("\nTAIL_END");
    }
};
}

int main()
{
    Agent::ToolRegistry registry;
    registry.registerTool(QSharedPointer<LlmTools::LlmTool>(new LargeResultTool));

    QString fullResult;
    const QString modelResult = registry.execute(QStringLiteral("large_result"),
                                                 QStringLiteral("{}"), nullptr, &fullResult);
    if (!modelResult.endsWith(QStringLiteral("[内容已截断]"))
        || modelResult.contains(QStringLiteral("TAIL_END"))
        || !fullResult.endsWith(QStringLiteral("TAIL_END"))) {
        std::fprintf(stderr, "full tool result must survive the model output limit\n");
        return 1;
    }

    const QString errorResult = registry.execute(QStringLiteral("missing"),
                                                 QStringLiteral("{}"), nullptr, &fullResult);
    if (errorResult != fullResult || !fullResult.contains(QStringLiteral("unregistered tool"))) {
        std::fprintf(stderr, "error result must also be available to the UI\n");
        return 1;
    }

    return 0;
}
