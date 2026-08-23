#include "LocalizedTextUtils.h"

namespace lite::Support {
    namespace {
        /// Single-tag RFC 4647 Lookup. Returns the matched text via \p out and
        /// reports whether a translation was hit (distinct from "hit text equals
        /// default"). POSIX-style keys (containing '_') are never matched.
        bool lookupSingle(const QMap<QString, QString> &localized, const QString &tag,
                          QString *out) {
            if (localized.isEmpty() || tag.isEmpty())
                return false;

            auto range = tag.toLower();
            while (!range.isEmpty()) {
                for (auto it = localized.cbegin(); it != localized.cend(); ++it) {
                    if (it.key().contains(u'_'))
                        continue; // not a BCP 47 tag
                    if (it.key().toLower() == range) {
                        *out = it.value();
                        return true;
                    }
                }
                const auto pos = range.lastIndexOf(u'-');
                if (pos < 0)
                    break;
                range = range.left(pos);
            }
            return false;
        }
    } // namespace

    QString lookupLocalizedText(const QMap<QString, QString> &localized, const QString &defaultText,
                                const QString &bcp47Locale) {
        QString text;
        if (lookupSingle(localized, bcp47Locale, &text))
            return text;
        return defaultText;
    }

    QString lookupLocalizedText(const QMap<QString, QString> &localized, const QString &defaultText,
                                const QStringList &bcp47LocaleCandidates) {
        for (const auto &tag : bcp47LocaleCandidates) {
            QString text;
            if (lookupSingle(localized, tag, &text))
                return text;
        }
        return defaultText;
    }

    bool hasLocalizedTexts(const QMap<QString, QString> &localized) {
        for (auto it = localized.cbegin(); it != localized.cend(); ++it) {
            if (!it.key().contains(u'_'))
                return true;
        }
        return false;
    }

} // namespace lite::Support
