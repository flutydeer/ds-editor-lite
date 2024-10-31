#ifndef G2PLISTWIDGET_H
#define G2PLISTWIDGET_H

#include <QListWidget>

namespace LangSetting {

    class G2pListWidget final : public QListWidget {
        Q_OBJECT
    public:
        explicit G2pListWidget(QWidget *parent = nullptr);
        ~G2pListWidget() override;

    Q_SIGNALS:
        void shown();

    protected:
        void showEvent(QShowEvent *event) override;
    };

} // LangSetting

#endif // G2PLISTWIDGET_H
