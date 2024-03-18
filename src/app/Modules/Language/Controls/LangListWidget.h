#ifndef LANGTABLEWIDGETS_H
#define LANGTABLEWIDGETS_H

#include <QListWidget>

#include "../LangCommon.h"

namespace LangMgr {

    class LangListWidget final : public QListWidget {
        Q_OBJECT
    public:
        explicit LangListWidget(QWidget *parent = nullptr);
        ~LangListWidget() override;
    };

} // LangMgr

#endif // LANGTABLEWIDGETS_H
