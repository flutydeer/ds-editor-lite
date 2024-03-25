#include <QDebug>

#include <QCoreApplication>

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

    return 0;
}