#pragma once

#include <QImage>
#include <QString>

namespace QrCodeGenerator {
QImage create(const QString& text, QString* error = nullptr);
QString save(const QString& text, const QString& directory, QString* error = nullptr);
}
