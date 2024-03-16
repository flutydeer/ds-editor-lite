#ifndef LANGMGRWIDGET_H
#define LANGMGRWIDGET_H

#include <QWidget>
#include "../Controls/LangTableWidget.h"

#include <QVBoxLayout>
#include "UI/Controls/Button.h"

#include <QLabel>

namespace LangMgr {

    class LangMgrWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit LangMgrWidget(QWidget *parent = nullptr);
        ~LangMgrWidget() override;

    private:
        LangTableWidget *m_langTableWidget;

        QHBoxLayout *m_mainLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_buttonLayout;

        QVBoxLayout *m_labelLayout;

        Button *m_applyButton;
        QLabel *m_label;
    };

} // LangMgr

#endif // LANGMGRWIDGET_H
