#include <QDebug>
#include <qrandom.h>
#include <QCoreApplication>
#include <algorithm>

#include <LangMgr/ILanguageManager.h>
#include <G2pMgr/IG2pManager.h>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const auto g2pMgr = G2pMgr::IG2pManager::instance();
    const auto langMgr = LangMgr::ILanguageManager::instance();

    QString errorMsg;
    g2pMgr->initialize(errorMsg);

    qDebug() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr->initialized();

    langMgr->initialize(errorMsg);
    qDebug() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    const QStringList testId = {"Mandarin", "Pinyin", "Number", "Space", "Linebreak", "Slur"};
    langMgr->setDefaultOrder(testId);

    QList<LangNote *> langNotes;

    for (const auto &langId : testId) {
        const int lenth = QRandomGenerator::global()->bounded(100, 200);
        const auto factory = langMgr->language(langId);

        for (int i = 0; i < lenth; i++) {
            const auto note = new LangNote();
            note->lyric = factory->randString();
            note->standard = langId;
            langNotes.append(note);
        }
    }

    std::mt19937 gen(std::random_device{}());
    std::shuffle(langNotes.begin(), langNotes.end(), gen);

    langMgr->correct(langNotes);

    for (const auto &note : langNotes) {
        if (note->language != note->standard) {
            qDebug() << "lyric: " << note->lyric << " standard: " << note->standard
                     << " res: " << note->language;
        }
    }

    qDebug() << "LangMgrTest: success";

    return 0;
}