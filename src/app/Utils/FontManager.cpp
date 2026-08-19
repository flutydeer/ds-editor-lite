//
// Font Manager for loading and managing custom fonts
//

#include "FontManager.h"
#include <lite/Support/Log.h>

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QFontDatabase>
#include <QMenu>
#include <QMenuBar>
#include <QStyle>
#include <QWidget>

FontManager &FontManager::instance() {
    static FontManager instance;
    return instance;
}

FontManager::FontManager() {
    // Use the system default font family as fallback; Microsoft Yahei UI is
    // absent on non-Chinese systems and must not be hard-coded here.
    m_fallbackFontFamily = QApplication::font().family();

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

const QString &FontManager::fallbackFontFamily() const {
    return m_fallbackFontFamily;
}

void FontManager::applyInterfaceFont(const QString &family) {
    const auto trimmedFamily = family.trimmed();
    QFont font = trimmedFamily.isEmpty() ? QFont(m_fallbackFontFamily) : QFont(trimmedFamily);
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setPixelSize(13);
    QApplication::setFont(font);

    // Propagate the change to every existing widget. The menu bar keeps its
    // widget-level font as a resolved cache that QApplication::setFont does
    // not refresh, so push the new font onto menu surfaces explicitly;
    // everywhere else a re-polish suffices to re-resolve QSS font styles.
    const auto widgets = QApplication::allWidgets();
    for (auto *widget : widgets) {
        if (!widget)
            continue;
        const bool isMenuSurface =
            qobject_cast<QMenuBar *>(widget) || qobject_cast<QMenu *>(widget);
        if (isMenuSurface && !widget->testAttribute(Qt::WA_SetFont))
            widget->setFont(font);
        if (auto *st = widget->style()) {
            st->unpolish(widget);
            st->polish(widget);
        }
        widget->update();
    }
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
