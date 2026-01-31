//
// Font Manager for loading and managing custom fonts
//

#include "FontManager.h"
#include "SystemUtils.h"
#include "Log.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QFontDatabase>

FontManager &FontManager::instance() {
    static FontManager instance;
    return instance;
}

FontManager::FontManager() {
    // Set fallback font based on platform
    if (SystemUtils::isWindows()) {
        m_fallbackFontFamily = "Microsoft Yahei UI";
    } else if (SystemUtils::productType() == SystemUtils::SystemProductType::MacOS) {
        m_fallbackFontFamily = "PingFang SC";
    } else {
        m_fallbackFontFamily = QApplication::font().family();
    }

    loadMusicFont();
}

void FontManager::loadMusicFont() {
    // Try to load the Sarasa-UI-Music-Regular font from Qt resources
    const QString fontPath = ":/fonts/Sarasa-UI-Music-Regular.ttf";

    if (!QFile::exists(fontPath)) {
        qWarning() << "FontManager: Music font file not found in resources at" << fontPath;
        qWarning() << "FontManager: Falling back to default font:" << m_fallbackFontFamily;
        m_musicFontLoaded = false;
        return;
    }

    const int fontId = QFontDatabase::addApplicationFont(fontPath);

    if (fontId == -1) {
        qWarning() << "FontManager: Failed to load music font from" << fontPath;
        qWarning() << "FontManager: Falling back to default font:" << m_fallbackFontFamily;
        m_musicFontLoaded = false;
        return;
    }

    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);

    if (fontFamilies.isEmpty()) {
        qWarning() << "FontManager: No font families found in" << fontPath;
        qWarning() << "FontManager: Falling back to default font:" << m_fallbackFontFamily;
        m_musicFontLoaded = false;
        return;
    }

    m_musicFontFamily = fontFamilies.first();
    m_musicFontLoaded = true;

    qInfo() << "FontManager: Successfully loaded music font:" << m_musicFontFamily;
}

QFont FontManager::musicUIFont(int pixelSize) const {
    QFont font;

    if (m_musicFontLoaded) {
        font.setFamily(m_musicFontFamily);
    } else {
        font.setFamily(m_fallbackFontFamily);
    }

    font.setPixelSize(pixelSize);
    font.setHintingPreference(QFont::PreferNoHinting);

    return font;
}

bool FontManager::isMusicFontLoaded() const {
    return m_musicFontLoaded;
}
