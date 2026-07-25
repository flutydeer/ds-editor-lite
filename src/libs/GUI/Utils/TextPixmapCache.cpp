//
// Created by FlutyDeer on 2026/4/10.
//

#include <lite/GUI/Utils/TextPixmapCache.h>

#include <QtGlobal>

TextPixmapCache *TextPixmapCache::instance() {
    static TextPixmapCache obj;
    return &obj;
}

bool TextPixmapCache::Key::operator==(const Key &other) const {
    return text == other.text && font == other.font && 
           color == other.color && qFuzzyCompare(devicePixelRatio, other.devicePixelRatio);
}

QPixmap TextPixmapCache::get(const Key &key) const {
    return m_cache.value(key);
}

void TextPixmapCache::insert(const Key &key, const QPixmap &pixmap) {
    m_cache.insert(key, pixmap);
}

bool TextPixmapCache::contains(const Key &key) const {
    return m_cache.contains(key);
}

void TextPixmapCache::clear() {
    m_cache.clear();
}
