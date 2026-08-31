#pragma once

#include <QString>
#include <vector>

struct LlmConfig {
    QString endpoint;
    QString apiKey;
    QString model;
};

struct FillResult {
    int controlIndex = -1;
    QString value;
};

// LLM client used by the AI Fill feature. Ported from wtl_clipboard's
// LlmClient: uses QNetworkAccessManager (instead of WinHTTP) and QJsonDocument
// (instead of nlohmann::json) but keeps the same request/response contract.
class LlmClient {
public:
    explicit LlmClient(const LlmConfig& config);

    std::vector<FillResult> RequestFill(const QString& clipboardText,
                                        const QString& controlsJson);
    QString LastError() const { return lastError_; }

private:
    QByteArray BuildRequestBody(const QString& clipboardText,
                                const QString& controlsJson) const;
    QByteArray HttpPost(const QString& fullUrl, const QByteArray& body);
    std::vector<FillResult> ParseResponse(const QByteArray& responseJson);

    LlmConfig config_;
    QString lastError_;
};
