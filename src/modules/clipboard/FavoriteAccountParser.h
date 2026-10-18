#pragma once
#include <QString>

struct FavoriteAccountFields {
    QString username;
    QString password;
    QString url;
};

FavoriteAccountFields ParseFavoriteAccount(const QString& text);
