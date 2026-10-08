#include "PronunciationText.h"

#include <algorithm>

namespace PronunciationText {

    std::string toUtf8(const QString &value) {
        const auto bytes = value.toUtf8();
        return {bytes.constData(), static_cast<size_t>(bytes.size())};
    }

    QString fromUtf8(const std::string &value) {
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    QStringList normalizeCandidates(const QString &pronunciation, QStringList candidates) {
        if (pronunciation.isEmpty())
            return candidates;
        const auto pronTokens = pronunciation.split(u' ', Qt::SkipEmptyParts);
        if (pronTokens.isEmpty())
            return candidates;
        for (auto &c : candidates)
            c = c.trimmed();
        candidates.removeAll(QString());
        const bool allArePronTokens =
            !candidates.isEmpty() &&
            std::all_of(candidates.cbegin(), candidates.cend(), [&](const QString &c) {
                return c.contains(u' ') ? c == pronunciation : pronTokens.contains(c);
            });
        if (allArePronTokens)
            return {pronunciation};
        return candidates;
    }

}
