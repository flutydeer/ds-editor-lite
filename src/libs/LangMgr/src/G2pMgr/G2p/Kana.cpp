#include "Kana.h"

#include <cpp-kana/Kana.h>

namespace LangMgr {
    KanaG2p::KanaG2p(QObject *parent) : IG2pFactory("ja-kana", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Kana"));
        setDescription(tr("Kana to Romanization converter."));
    }

    KanaG2p::~KanaG2p() = default;

    static std::vector<std::string> toStdVector(const QStringList &input) {
        std::vector<std::string> result;
        result.reserve(input.size());
        for (const auto &str : input)
            result.push_back(str.toUtf8().toStdString());
        return result;
    }

    QList<LangNote> KanaG2p::convert(const QStringList &input, const QJsonObject *config) const {
        Q_UNUSED(config);

        QList<LangNote> result;
        const auto g2pRes = Kana::kanaToRomaji(toStdVector(input),Kana::Error::Default, false);
        for (int i = 0; i < g2pRes.size(); i++) {
            LangNote langNote;
            langNote.lyric = input[i];
            langNote.syllable = QString::fromUtf8(g2pRes[i].romaji);
            langNote.candidates = QStringList() << langNote.syllable;
            langNote.error = g2pRes[i].error;
            result.append(langNote);
        }
        return result;
    }

    QJsonObject KanaG2p::config() {
        return {};
    }
} // LangMgr