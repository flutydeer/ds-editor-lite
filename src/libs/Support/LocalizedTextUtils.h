#ifndef LOCALIZEDTEXTUTILS_H
#define LOCALIZEDTEXTUTILS_H

#include <QMap>
#include <QString>
#include <QStringList>

namespace lite::Support {

    /// Resolves the text for a BCP 47 locale tag using RFC 4647 Lookup:
    /// try the full tag first, then repeatedly strip the rightmost subtag
    /// (separator '-'), matching case-insensitively, and finally fall back to
    /// the default text. Keys that are not BCP 47 tags (e.g. POSIX-style
    /// "zh_CN") are kept in the table but never matched, mirroring
    /// srt::core::DisplayText::text(locale) so the host resolves localized
    /// names the same way synthrt does.
    [[nodiscard]] QString lookupLocalizedText(const QMap<QString, QString> &localized,
                                              const QString &defaultText,
                                              const QString &bcp47Locale);

    /// RFC 4647 Lookup over a list of preferred BCP 47 tags (e.g. Qt's
    /// QLocale::uiLanguages(): "zh-Hans-CN", "zh-CN", "zh-Hans", "zh"). Each
    /// candidate is resolved by the single-tag lookup above and the first hit
    /// wins; otherwise the default text is returned.
    [[nodiscard]] QString lookupLocalizedText(const QMap<QString, QString> &localized,
                                              const QString &defaultText,
                                              const QStringList &bcp47LocaleCandidates);

    /// True when \p localized contains no non-default BCP 47 translations.
    [[nodiscard]] bool hasLocalizedTexts(const QMap<QString, QString> &localized);

} // namespace lite::Support

#endif // LOCALIZEDTEXTUTILS_H
