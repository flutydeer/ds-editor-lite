#include "Unknown.h"

namespace LangMgr {
    Unknown::Unknown(QObject *parent) : IG2pFactory("unknown", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Unknown"));
        setDescription(tr("Unknown language, no conversion required."));
    }

    Unknown::~Unknown() = default;

    QList<LangNote> Unknown::convert(const QStringList &input, const QJsonObject *config) const {
        Q_UNUSED(config);

        QList<LangNote> result;
        for (const auto &i : input) {
            LangNote langNote;
            langNote.lyric = i;
            langNote.syllable = i;
            langNote.candidates = QStringList() << i;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject Unknown::config() {
        return {};
    }
} // LangMgr