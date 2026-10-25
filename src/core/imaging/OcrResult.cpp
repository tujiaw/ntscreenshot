#include "OcrResult.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

namespace {
QPolygonF parsePolygon(const QJsonValue& value)
{
    const QJsonArray points = value.toArray();
    QPolygonF polygon;
    if (points.size() == 4 && points[0].isDouble()) {
        for (const auto& point : points) {
            if (!point.isDouble() || !std::isfinite(point.toDouble())) return {};
        }
        const QRectF rect(QPointF(points[0].toDouble(), points[1].toDouble()),
                          QPointF(points[2].toDouble(), points[3].toDouble()));
        if (!rect.isValid()) return {};
        polygon = QPolygonF(rect);
    } else {
        for (const auto& point : points) {
            const QJsonArray pair = point.toArray();
            if (pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble() ||
                !std::isfinite(pair[0].toDouble()) || !std::isfinite(pair[1].toDouble())) return {};
            polygon << QPointF(pair[0].toDouble(), pair[1].toDouble());
        }
    }
    return polygon.size() >= 4 && polygon.boundingRect().isValid() ? polygon : QPolygonF();
}

void appendResult(const QJsonValue& value, OcrResult& result)
{
    if (value.isArray()) {
        for (const auto& item : value.toArray()) appendResult(item, result);
        return;
    }
    if (!value.isObject()) return;
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("rec_texts")).isArray()) {
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) appendResult(it.value(), result);
        return;
    }
    auto at = [](const QJsonArray& array, int i) { return i < array.size() ? array.at(i) : QJsonValue(); };
    const auto texts = object.value(QStringLiteral("rec_texts")).toArray();
    const auto polygons = object.value(QStringLiteral("rec_polys")).toArray();
    const auto boxes = object.value(QStringLiteral("rec_boxes")).toArray();
    const auto words = object.value(QStringLiteral("text_word")).toArray();
    const auto wordBoxes = object.value(QStringLiteral("text_word_boxes")).toArray();
    const auto wordPolygons = object.value(QStringLiteral("text_word_region")).toArray();
    for (int i = 0; i < texts.size(); ++i) {
        const QString text = texts[i].toString();
        if (text.trimmed().isEmpty()) continue;
        const int line = result.lines.size();
        result.lines.push_back(text);
        const auto tokens = at(words, i).toArray();
        auto positions = at(wordBoxes, i).toArray();
        if (positions.isEmpty()) positions = at(wordPolygons, i).toArray();
        QVector<OcrSpan> spans;
        int offset = 0;
        if (!tokens.isEmpty() && tokens.size() == positions.size()) {
            for (int j = 0; j < tokens.size(); ++j) {
                const QString token = tokens[j].toString();
                const int start = token.isEmpty() ? -1 : text.indexOf(token, offset);
                const auto polygon = parsePolygon(positions[j]);
                if (start < 0 || polygon.isEmpty() || !text.mid(offset, start - offset).trimmed().isEmpty()) {
                    spans.clear();
                    break;
                }
                spans.push_back({polygon, line, start, static_cast<int>(token.size())});
                offset = start + token.size();
            }
            if (!text.mid(offset).trimmed().isEmpty()) spans.clear();
        }
        if (!spans.isEmpty()) {
            result.spans += spans;
        } else {
            auto polygon = parsePolygon(at(polygons, i));
            if (polygon.isEmpty()) polygon = parsePolygon(at(boxes, i));
            if (!polygon.isEmpty()) result.spans.push_back({polygon, line, 0, static_cast<int>(text.size())});
        }
    }
}
}

OcrResult OcrResult::parse(const QByteArray& content)
{
    OcrResult result;
    auto append = [&](const QByteArray& bytes) {
        const auto document = QJsonDocument::fromJson(bytes);
        if (document.isObject()) appendResult(document.object(), result);
        else if (document.isArray()) appendResult(document.array(), result);
        return !document.isNull();
    };
    if (!append(content)) {
        for (const auto& line : content.split('\n')) {
            if (!line.trimmed().isEmpty()) append(line);
        }
    }
    return result;
}

QString OcrResult::selectedText(int first, int last) const
{
    if (first < 0 || last < 0 || first >= spans.size() || last >= spans.size()) return {};
    if (first > last) std::swap(first, last);
    QStringList parts;
    for (int i = first; i <= last;) {
        const auto& begin = spans[i];
        int end = i;
        while (end < last && spans[end + 1].line == begin.line) ++end;
        parts.push_back(lines[begin.line].mid(begin.start,
            spans[end].start + spans[end].length - begin.start));
        i = end + 1;
    }
    return parts.join(QLatin1Char('\n'));
}
