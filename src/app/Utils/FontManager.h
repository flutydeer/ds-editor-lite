//
// Font Manager for loading and managing custom fonts
//

#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QFont>
#include <QString>
#include <QFontDatabase>

class FontManager {
public:
    static FontManager &instance();

    // Apply a global interface font. Passing an empty family restores the
    // platform-default UI font. Hot-swappable at runtime (no restart).
    void applyInterfaceFont(const QString &family);

    // Get the platform-default UI font family
    const QString &fallbackFontFamily() const;

    // Get the music UI font (Sarasa-UI-Music-Regular) with fallback
    QFont musicUIFont(int pixelSize = 13) const;

    // Check if the custom font is loaded
    bool isMusicFontLoaded() const;

private:
    FontManager();
    ~FontManager() = default;

    FontManager(const FontManager &) = delete;
    FontManager &operator=(const FontManager &) = delete;

    void loadMusicFont();

    bool m_musicFontLoaded = false;
    QString m_musicFontFamily;
    QString m_fallbackFontFamily;
};

#endif // FONTMANAGER_H
