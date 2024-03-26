#ifndef LANGTABLEWIDGETS_H
#define LANGTABLEWIDGETS_H

#include <QListWidget>

#include <LangCommon.h>

#include <QLabel>

namespace LangSetting {

    class LangListWidget final : public QListWidget {
        Q_OBJECT
    public:
        explicit LangListWidget(QWidget *parent = nullptr);
        ~LangListWidget() override;

        [[nodiscard]] QStringList langOrder() const;

    Q_SIGNALS:
        void priorityChanged();

    protected:
        void dropEvent(QDropEvent *event) override;
    };

} // LangMgr

#endif // LANGTABLEWIDGETS_H
