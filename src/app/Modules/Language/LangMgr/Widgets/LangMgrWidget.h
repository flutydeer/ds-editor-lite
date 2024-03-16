#ifndef LANGMGRWIDGET_H
#define LANGMGRWIDGET_H

#include <QLabel>
#include <QWidget>

#include "UI/Controls/Button.h"

#include "../Controls/LangTableWidget.h"
#include "../Controls/LangInfoWidget.h"

namespace LangMgr {

    class LangMgrWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit LangMgrWidget(QWidget *parent = nullptr);
        ~LangMgrWidget() override;

    private:
        LangTableWidget *m_langTableWidget;
        LangInfoWidget *m_langInfoWidget;

        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_centerLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_buttonLayout;

        QVBoxLayout *m_labelLayout;

        Button *m_applyButton;
        QLabel *m_label;
    };

} // LangMgr

#endif // LANGMGRWIDGET_H
