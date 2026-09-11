#pragma once

#include "ClipItem.h"

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
        std::stable_partition(items_.begin(), items_.end(), [](const ClipItem& item) { return item.pinned; });
        Trim();
    }

    bool Empty() const {
        return items_.empty();
    }

    void Clear() {
        items_.clear();
    }

    bool RemoveAt(size_t index) {
        if (index >= items_.size()) {
            return false;
        }
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool TogglePinned(size_t index) {
        if (index >= items_.size()) return false;
        ClipItem item = std::move(items_[index]);
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
        item.pinned = !item.pinned;
        const auto position = item.pinned ? items_.begin() : FirstUnpinned();
        items_.insert(position, std::move(item));
        Trim();
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

        auto it = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& item) {
            return item.kind == ClipKind::Text && item.text == text;
        });
        if (it != items_.end()) {
            if (promoteExisting && !it->pinned) {
                ClipItem existing = std::move(*it);
                existing.capturedAt = QDateTime::currentDateTime();
                items_.erase(it);
                items_.insert(FirstUnpinned(), std::move(existing));
            }
            return true;
        }

        ClipItem item;
        item.kind = ClipKind::Text;
        item.capturedAt = QDateTime::currentDateTime();
        item.text = std::move(text);
        items_.insert(FirstUnpinned(), std::move(item));
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

        auto matchIt = items_.end();
        if (pixelHash != 0) {
            matchIt = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& existing) {
                return existing.kind == ClipKind::Image && existing.pixelHash == pixelHash;
            });
        } else {
            matchIt = std::find_if(items_.begin(), items_.end(), [&](const ClipItem& existing) {
                return existing.kind == ClipKind::Image && existing.data == item.data;
            });
        }
        if (matchIt != items_.end()) {
            if (promoteExisting && !matchIt->pinned) {
                ClipItem existing = std::move(*matchIt);
                existing.capturedAt = QDateTime::currentDateTime();
                items_.erase(matchIt);
                items_.insert(FirstUnpinned(), std::move(existing));
            }
            return true;
        }

        items_.insert(FirstUnpinned(), std::move(item));
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
        if (left.pixelHash != 0 && right.pixelHash != 0) {
            return left.pixelHash == right.pixelHash;
        }
        return left.data == right.data;
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
    std::vector<ClipItem>::iterator FirstUnpinned() {
        return std::find_if(items_.begin(), items_.end(), [](const ClipItem& item) { return !item.pinned; });
    }

    void Trim() {
        const size_t pinnedCount = static_cast<size_t>(std::distance(items_.begin(), FirstUnpinned()));
        if (items_.size() - pinnedCount > maxItems_) {
            items_.resize(pinnedCount + maxItems_);
        }
    }

    size_t maxItems_ = 50;
    std::vector<ClipItem> items_;
};
