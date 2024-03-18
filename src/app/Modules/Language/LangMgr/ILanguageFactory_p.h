#ifndef ILANGUAGEFACTORY_P_H
#define ILANGUAGEFACTORY_P_H

#include "ILanguageFactory.h"

#include <QCheckBox>

#include "UI/Controls/ComboBox.h"

#include "../G2pMgr/IG2pManager.h"

#include <QObject>
#include <QLabel>
#include <QVBoxLayout>

namespace LangMgr {

    class ILanguageFactoryPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(ILanguageFactory)
    public:
        ILanguageFactoryPrivate();
        ~ILanguageFactoryPrivate() override;

        void init();

        ILanguageFactory *q_ptr;

        QString id;

        bool enabled = true;

        bool discardResult = false;

        QString displayName;
        QString description;

        QString author;
        QString displayCategory;

        QString m_selectedG2p;
    };

}

#endif // ILANGUAGEFACTORY_P_H
