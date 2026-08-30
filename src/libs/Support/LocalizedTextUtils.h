#ifndef LOCALIZEDTEXTUTILS_H
#define LOCALIZEDTEXTUTILS_H

#include <QMap>
#include <QString>
#include <QStringList>

namespace lite::Support {

    /// Resolves the text for a requested language tag by matching it against
    /// the keys of \p localized with ICU (IcuWrapper::bestMatch): tags are
    /// normalized (case, POSIX-style '_' separators, ...) before matching, and
    /// the matched key keeps its original spelling so the value is fetched by
    /// exact map key. Returns \p defaultText when nothing matches.
    ///
    /// Matching policy is a frontend concern by design (ds-spec 2.4):
    /// srt::core::DisplayText itself only stores keys opaquely and serves
    /// exact, case-sensitive lookups.
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
