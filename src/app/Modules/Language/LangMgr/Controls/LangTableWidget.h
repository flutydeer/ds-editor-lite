#ifndef LANGTABLEWIDGETS_H
#define LANGTABLEWIDGETS_H

#include <QTableWidget>

#include "../../LangCommon.h"

namespace LangMgr {

    class LangTableWidget final : public QTableWidget {
        Q_OBJECT
    public:
        explicit LangTableWidget(QWidget *parent = nullptr);
        ~LangTableWidget() override;

    protected:
        void dropEvent(QDropEvent *event) override;

        void setCellCheckBox(const int &row, const int &column, const Qt::CheckState &checkState,
                             const QString &text = "");

        [[nodiscard]] bool cellCheckState(const int &row, const int &column) const;

        void fillRow(const int &row, const LangConfig &langConfig);

        [[nodiscard]] LangConfig exportConfig(const int &row) const;
    };

} // LangMgr

#endif // LANGTABLEWIDGETS_H
