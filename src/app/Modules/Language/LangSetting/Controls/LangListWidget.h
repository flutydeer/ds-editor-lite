#ifndef LANGTABLEWIDGETS_H
#define LANGTABLEWIDGETS_H

#include <QListWidget>

namespace LangSetting {

    class LangListWidget final : public QListWidget {
        Q_OBJECT
    public:
        explicit LangListWidget(QWidget *parent = nullptr);
        ~LangListWidget() override;

        [[nodiscard]] QStringList langOrder() const;

    Q_SIGNALS:
        void priorityChanged();
        void shown();

    protected:
        void showEvent(QShowEvent *event) override;
        void dropEvent(QDropEvent *event) override;
    };

} // LangMgr

#endif // LANGTABLEWIDGETS_H
