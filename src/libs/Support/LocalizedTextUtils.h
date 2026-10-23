#ifndef LOCALIZEDTEXTUTILS_H
#define LOCALIZEDTEXTUTILS_H

#include <QMap>
#include <QString>
#include <QStringList>

namespace lite::Support {

    /// Match language tags with ICU while preserving the original map keys.
    /// Return defaultText if no key matches; srt::DisplayText itself uses exact lookup.
    [[nodiscard]] QString lookupLocalizedText(const QMap<QString, QString> &localized,
                                              const QString &defaultText,
                                              const QString &bcp47Locale);

    /// Resolves against a list of preferred tags in priority order (e.g. Qt's
    /// QLocale::uiLanguages(): "zh-Hans-CN", "zh-CN", "zh-Hans", "zh"). The
    /// first candidate with an ICU hit wins; otherwise the default text is
    /// returned.
    [[nodiscard]] QString lookupLocalizedText(const QMap<QString, QString> &localized,
                                              const QString &defaultText,
                                              const QStringList &bcp47LocaleCandidates);

} // namespace lite::Support

#endif // LOCALIZEDTEXTUTILS_H
