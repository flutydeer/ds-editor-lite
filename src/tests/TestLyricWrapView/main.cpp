#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>

#include <qrandom.h>
#include <QCoreApplication>
#include <algorithm>

#include <LangMgr/ILanguageManager.h>
#include <G2pMgr/IG2pManager.h>

#include "LyricWrapView.h"

using namespace LyricWrap;

int main(int argc, char **argv) {
    QApplication a(argc, argv);
    QMainWindow win;
    const auto widget = new QWidget();
    const auto lyricWrapWidget = new LyricWrapView();
    const auto mainlayout = new QVBoxLayout();
    mainlayout->addWidget(lyricWrapWidget);
    widget->setLayout(mainlayout);
    win.setCentralWidget(widget);
    win.setMinimumSize(300, 200);

    const auto g2pMgr = G2pMgr::IG2pManager::instance();
    const auto langMgr = LangMgr::ILanguageManager::instance();

    QString errorMsg;
    g2pMgr->initialize(errorMsg);

    qDebug() << "G2pMgr: errorMsg" << errorMsg << "initialized:" << g2pMgr->initialized();

    langMgr->initialize(errorMsg);
    qDebug() << "LangMgr: errorMsg" << errorMsg << "initialized:" << langMgr->initialized();

    const QStringList testId = {"Mandarin", "English", "Number", "Slur"};
    langMgr->setDefaultOrder(testId);

    QList<LangNote *> langNotes;

    for (const auto &langId : testId) {
        const int lenth = QRandomGenerator::global()->bounded(100, 101);
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
    langMgr->convert(langNotes);

    const int middleIndex = langNotes.size() / 2;
    const QList<LangNote *> firstHalf = langNotes.mid(0, middleIndex);
    const QList<LangNote *> secondHalf = langNotes.mid(middleIndex);

    lyricWrapWidget->appendList(firstHalf);
    lyricWrapWidget->appendList(secondHalf);

    win.show();
    return QApplication::exec();
}
