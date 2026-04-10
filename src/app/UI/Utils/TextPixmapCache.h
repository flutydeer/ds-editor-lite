//
// Created by FlutyDeer on 2026/4/10.
//

#ifndef TEXTPIXMAPCACHE_H
#define TEXTPIXMAPCACHE_H

#include "Utils/Singleton.h"

#include <QColor>
#include <QFont>
#include <QMap>
#include <QPixmap>
#include <QString>

class TextPixmapCache {
public:
    struct Key {
        QString text;
        QFont font;
        QColor color;
        double devicePixelRatio;

        QString toKeyString() const;
    };

    LITE_SINGLETON_DECLARE_INSTANCE(TextPixmapCache)
    Q_DISABLE_COPY_MOVE(TextPixmapCache)

    QPixmap get(const Key &key);
    void insert(const Key &key, const QPixmap &pixmap);
    bool contains(const Key &key) const;
    void clear();

private:
    TextPixmapCache() = default;
    ~TextPixmapCache() = default;

    QMap<QString, QPixmap> m_cache;
};

#endif // TEXTPIXMAPCACHE_H
