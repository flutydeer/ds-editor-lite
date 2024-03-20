#ifndef ILANGUAGEFACTORY_P_H
#define ILANGUAGEFACTORY_P_H

#include "ILanguageFactory.h"

#include "UI/Controls/ComboBox.h"

#include <QObject>
#include <QLabel>
#include <QJsonObject>

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

        QString categroy;
        QString description;
        QString displayName;
        QString author;
        QString displayCategory;

        QString m_selectedG2p;
        QJsonObject *m_g2pConfig;
    };

}

#endif // ILANGUAGEFACTORY_P_H
