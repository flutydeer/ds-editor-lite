#ifndef IG2PPRIVATE_H
#define IG2PPRIVATE_H

#include <QObject>

#include <LangMgr/IG2pManager.h>

namespace LangMgr {

    class IG2pManagerPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(IG2pManager)

    public:
        IG2pManagerPrivate();
        ~IG2pManagerPrivate() override;

        IG2pManager *q_ptr;

        QMap<QString, IG2pFactory *> g2ps;

        bool initialized = false;

        QStringList baseG2p = {"slur", "linebreak", "number", "space", "punctuation"};
    };

}

#endif // IG2PPRIVATE_H