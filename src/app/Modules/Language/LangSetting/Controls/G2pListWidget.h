#ifndef G2PLISTWIDGET_H
#define G2PLISTWIDGET_H

#include <QListWidget>

namespace LangSetting {

    class GListWidget final : public QListWidget {
        Q_OBJECT
    public:
        explicit GListWidget(QWidget *parent = nullptr);
        ~GListWidget() override;

        static void copyItem(int row);
        void deleteItem(int row);

        void updateDeleteButtonState();

    Q_SIGNALS:
        void shown();
        void deleteButtonStateChanged(bool canDelete);

    protected:
        void showEvent(QShowEvent *event) override;
    };

    class G2pListWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit G2pListWidget(QWidget *parent = nullptr);
        ~G2pListWidget() override;

        QString currentG2pId() const;

        GListWidget *m_gListWidget;

    private:
        void copySelectedItem() const;
        void deleteSelectedItem() const;
    };

} // LangSetting

#endif // G2PLISTWIDGET_H
