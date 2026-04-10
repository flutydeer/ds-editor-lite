//
// Created by FlutyDeer on 2026/4/10.
//

#include "TextPixmapCache.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

LITE_SINGLETON_IMPLEMENT_INSTANCE(TextPixmapCache)

QString TextPixmapCache::Key::toKeyString() const {
    auto obj = QJsonObject{
        {"text",             text                           },
        {"fontFamily",       font.family()                  },
        {"fontSize",         font.pointSize()               },
        {"fontWeight",       static_cast<int>(font.weight())},
        {"color",            color.name(QColor::HexArgb)    },
        {"devicePixelRatio", devicePixelRatio               }
    };
    const QByteArray jsonData = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QByteArray hashData = QCryptographicHash::hash(jsonData, QCryptographicHash::Sha1);
    return hashData.toHex();
}

QPixmap TextPixmapCache::get(const Key &key) {
    return m_cache.value(key.toKeyString());
}

void TextPixmapCache::insert(const Key &key, const QPixmap &pixmap) {
    m_cache.insert(key.toKeyString(), pixmap);
}

bool TextPixmapCache::contains(const Key &key) const {
    return m_cache.contains(key.toKeyString());
}

void TextPixmapCache::clear() {
    m_cache.clear();
}
