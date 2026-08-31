#include "modules/assistant/runtime/agent/ConversationHistory.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>

namespace {

QJsonObject userMsg(const QString &text)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("user"));
    msg.insert(QStringLiteral("content"), text);
    return msg;
}

QJsonObject assistantText(const QString &text)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    msg.insert(QStringLiteral("content"), text);
    return msg;
}

QJsonObject assistantToolCall(const QString &id, const QString &name)
{
    QJsonObject func;
    func.insert(QStringLiteral("name"), name);
    func.insert(QStringLiteral("arguments"), QStringLiteral("{}"));
    QJsonObject call;
    call.insert(QStringLiteral("id"), id);
    call.insert(QStringLiteral("type"), QStringLiteral("function"));
    call.insert(QStringLiteral("function"), func);
    QJsonArray calls;
    calls.append(call);
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    msg.insert(QStringLiteral("content"), QJsonValue());
    msg.insert(QStringLiteral("tool_calls"), calls);
    return msg;
}

QJsonObject toolMsg(const QString &id, const QString &content)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("tool"));
    msg.insert(QStringLiteral("tool_call_id"), id);
    msg.insert(QStringLiteral("content"), content);
    return msg;
}

bool fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return false;
}

} // namespace

int main()
{
    {
        QJsonArray small;
        small.append(userMsg(QStringLiteral("hi")));
        small.append(assistantText(QStringLiteral("hello")));
        const auto result = ConversationHistory::trimToTokenBudget(small, 24000, -1);
        if (result.kept.size() != 2 || !result.droppedChunks.isEmpty()) {
            return fail("under-budget conversation must stay intact") ? 1 : 1;
        }
    }

    {
        QJsonArray messages;
        messages.append(userMsg(QStringLiteral("first")));
        messages.append(assistantToolCall(QStringLiteral("c1"), QStringLiteral("ls")));
        messages.append(toolMsg(QStringLiteral("c1"), QStringLiteral("dir listing")));
        messages.append(assistantText(QStringLiteral("listed")));
        messages.append(userMsg(QStringLiteral("second")));
        messages.append(assistantText(QStringLiteral("done")));

        const auto result = ConversationHistory::trimToTokenBudget(messages, 20, 100);
        if (result.kept.isEmpty()) {
            return fail("must keep at least the last round") ? 1 : 1;
        }

        bool sawOrphanTool = false;
        bool sawToolCallWithoutTools = false;
        QString pendingToolId;
        for (const QJsonValue &value : result.kept) {
            const QJsonObject msg = value.toObject();
            const QString role = msg.value(QStringLiteral("role")).toString();
            if (role == QStringLiteral("assistant")) {
                const QJsonArray calls = msg.value(QStringLiteral("tool_calls")).toArray();
                if (!calls.isEmpty()) {
                    pendingToolId = calls.at(0).toObject().value(QStringLiteral("id")).toString();
                }
            } else if (role == QStringLiteral("tool")) {
                const QString id = msg.value(QStringLiteral("tool_call_id")).toString();
                if (pendingToolId != id) {
                    sawOrphanTool = true;
                }
                pendingToolId.clear();
            }
        }
        if (!pendingToolId.isEmpty()) {
            sawToolCallWithoutTools = true;
        }
        if (sawOrphanTool || sawToolCallWithoutTools) {
            return fail("tool_call groups must be kept or dropped together") ? 1 : 1;
        }

        if (result.kept.first().toObject().value(QStringLiteral("content")).toString() == QStringLiteral("first")
            && result.droppedChunks.isEmpty()) {
            return fail("over-budget first round should be dropped as a whole") ? 1 : 1;
        }
    }

    {
        QJsonArray messages;
        messages.append(userMsg(QStringLiteral("only")));
        messages.append(assistantToolCall(QStringLiteral("c1"), QStringLiteral("ls")));
        messages.append(toolMsg(QStringLiteral("c1"), QStringLiteral("dir listing")));
        messages.append(assistantText(QStringLiteral("listed")));
        const auto result = ConversationHistory::trimToTokenBudget(messages, 1, 100000);
        if (result.kept.size() != messages.size() || !result.droppedChunks.isEmpty()) {
            return fail("single round must not be split even when over budget") ? 1 : 1;
        }
    }

    std::fprintf(stdout, "ConversationHistory contract passed\n");
    return 0;
}
