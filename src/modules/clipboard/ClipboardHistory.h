#pragma once

#include "ClipItem.h"
#include <QImage>

#include <algorithm>
#include <vector>

// In-memory clipboard history. Matches the behaviour of wtl_clipboard's
// ClipboardHistory (dedup, promote-on-duplicate, trim to max items).
class ClipboardHistory {
public:
    void SetMaxItems(size_t value) {
        maxItems_ = std::max<size_t>(1, value);
        Trim();
    }

    size_t MaxItems() const {
        return maxItems_;
    }

    const std::vector<ClipItem>& Items() const {
        return items_;
    }

    void ReplaceItems(std::vector<ClipItem> items) {
        items_ = std::move(items);
        std::stable_partition(items_.begin(), items_.end(), [](const ClipItem& item) { return item.favorite; });
        Trim();
    }

    bool Empty() const {
        return items_.empty();
    }

    void Clear() {
        items_.erase(std::remove_if(items_.begin(), items_.end(),
                                   [](const ClipItem& item) { return !item.favorite; }), items_.end());
    }

    bool RemoveAt(size_t index) {
        if (index >= items_.size()) {
            return false;
        }
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool ToggleFavorite(size_t index) {
        if (index >= items_.size()) return false;
        if (items_[index].favorite) return RemoveAt(index);
        ClipItem item = items_[index];
        const auto existing = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& candidate) {
            return candidate.favorite && SameContent(candidate, item);
        });
        if (existing != items_.end()) {
            existing->capturedAt = QDateTime::currentDateTime();
            if (item.pixelHash != 0) existing->pixelHash = item.pixelHash;
            return true;
        }
        item.favorite = true;
        item.capturedAt = QDateTime::currentDateTime();
        items_.insert(items_.begin(), std::move(item));
        return true;
    }

    bool UpdateText(size_t index, QString text) {
        if (index >= items_.size() || items_[index].kind != ClipKind::Text) {
            return false;
        }
        NormalizeText(text);
        if (text.isEmpty()) {
            return false;
        }
        items_[index].text = std::move(text);
        return true;
    }

    bool AddText(QString text, bool promoteExisting = true) {
        NormalizeText(text);
        if (text.isEmpty()) {
            return false;
        }

        const QDateTime copiedAt = QDateTime::currentDateTime();

        auto it = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& item) {
            return !item.favorite && item.kind == ClipKind::Text && item.text == text;
        });
        if (it != items_.end()) {
            if (promoteExisting && !it->favorite) {
                ClipItem existing = std::move(*it);
                existing.capturedAt = copiedAt;
                items_.erase(it);
                items_.insert(FirstHistoryItem(), std::move(existing));
            }
            return true;
        }

        ClipItem item;
        item.kind = ClipKind::Text;
        item.capturedAt = copiedAt;
        item.text = std::move(text);
        items_.insert(FirstHistoryItem(), std::move(item));
        Trim();
        return true;
    }

    bool AddImage(QByteArray data, int width = 0, int height = 0,
                  quint64 pixelHash = 0, bool promoteExisting = true) {
        if (data.isEmpty()) {
            return false;
        }

        ClipItem item;
        item.kind = ClipKind::Image;
        item.capturedAt = QDateTime::currentDateTime();
        item.data = std::move(data);
        item.width = width;
        item.height = height;
        item.pixelHash = pixelHash;

        auto matchIt = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& existing) {
            return !existing.favorite && SameContent(existing, item);
        });
        if (matchIt != items_.end()) {
            if (promoteExisting && !matchIt->favorite) {
                ClipItem existing = std::move(*matchIt);
                existing.capturedAt = item.capturedAt;
                if (item.pixelHash != 0) existing.pixelHash = item.pixelHash;
                items_.erase(matchIt);
                items_.insert(FirstHistoryItem(), std::move(existing));
            }
            return true;
        }

        items_.insert(FirstHistoryItem(), std::move(item));
        Trim();
        return true;
    }

    static bool SameContent(const ClipItem& left, const ClipItem& right) {
        if (left.kind != right.kind) {
            return false;
        }
        if (left.kind == ClipKind::Text) {
            return left.text == right.text;
        }
        if (left.pixelHash != 0 && right.pixelHash != 0
            && left.width > 0 && left.height > 0 && right.width > 0 && right.height > 0) {
            return left.width == right.width && left.height == right.height
                && left.pixelHash == right.pixelHash;
        }
        if (left.data == right.data) return true;
        // Legacy rows have no saved pixel hash. PNG metadata/encoding may differ
        // after copying the same pixels, so compare decoded content as fallback.
        const QImage leftImage = QImage::fromData(left.data);
        const QImage rightImage = QImage::fromData(right.data);
        return !leftImage.isNull() && !rightImage.isNull()
            && leftImage.convertToFormat(QImage::Format_ARGB32)
                == rightImage.convertToFormat(QImage::Format_ARGB32);
    }

    static QString DisplayText(const ClipItem& item) {
        if (item.kind == ClipKind::Text) {
            return item.text;
        }

        QString result = QStringLiteral("[Image] ");
        if (item.width > 0 && item.height > 0) {
            result += QStringLiteral("%1 x %2, ").arg(item.width).arg(item.height);
        }
        result += QStringLiteral("%1 KB").arg((item.data.size() + 1023) / 1024);
        return result;
    }

    static void NormalizeText(QString& text) {
        while (!text.isEmpty() &&
               (text.back() == QChar('\0') || text.back() == QChar('\r') ||
                text.back() == QChar('\n') || text.back() == QChar(' ') ||
                text.back() == QChar('\t'))) {
            text.chop(1);
        }
        int first = 0;
        while (first < text.size() &&
               (text[first] == QChar('\r') || text[first] == QChar('\n') ||
                text[first] == QChar(' ') || text[first] == QChar('\t'))) {
            ++first;
        }
        if (first > 0) {
            text.remove(0, first);
        }
    }

private:
    std::vector<ClipItem>::iterator FirstHistoryItem() {
        return std::find_if(items_.begin(), items_.end(), [](const ClipItem& item) { return !item.favorite; });
    }

    void Trim() {
        const size_t favoriteCount = static_cast<size_t>(std::distance(items_.begin(), FirstHistoryItem()));
        if (items_.size() - favoriteCount > maxItems_) {
            items_.resize(favoriteCount + maxItems_);
        }
    }

    size_t maxItems_ = 50;
    std::vector<ClipItem> items_;
};
