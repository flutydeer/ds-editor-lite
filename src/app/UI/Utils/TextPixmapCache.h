//
// Created by FlutyDeer on 2026/4/10.
//

#ifndef TEXTPIXMAPCACHE_H
#define TEXTPIXMAPCACHE_H

#include "Utils/Singleton.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QPixmap>
#include <QString>

class TextPixmapCache {
public:
    struct Key {
        QString text;
        QFont font;
        QColor color;
        double devicePixelRatio = 1.0;

        bool operator==(const Key &other) const;
    };

    LITE_SINGLETON_DECLARE_INSTANCE(TextPixmapCache)
    Q_DISABLE_COPY_MOVE(TextPixmapCache)

    QPixmap get(const Key &key) const;
    void insert(const Key &key, const QPixmap &pixmap);
    bool contains(const Key &key) const;
    void clear();

private:
    TextPixmapCache() = default;
    ~TextPixmapCache() = default;

    QHash<Key, QPixmap> m_cache;
};

inline size_t qHash(const TextPixmapCache::Key &key, size_t seed = 0) {
    return qHash(key.text, seed) ^ 
           qHash(key.font.family(), seed) ^ 
           qHash(key.font.pointSize(), seed) ^ 
           qHash(static_cast<int>(key.font.weight()), seed) ^ 
           qHash(key.color.rgba(), seed) ^ 
           qHash(static_cast<int>(key.devicePixelRatio * 1000), seed);
}

#endif // TEXTPIXMAPCACHE_H
