#ifndef G2PINFOWIDGET_H
#define G2PINFOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <QGroupBox>

namespace LangSetting {

    class G2pInfoWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit G2pInfoWidget(QWidget *parent = nullptr);
        ~G2pInfoWidget() override;

    Q_SIGNALS:
        void g2pConfigChanged() const;

    public Q_SLOTS:
        void setInfo(const QString &g2pId) const;

    private:
        void removeWidget() const;

        QVBoxLayout *m_mainLayout;
        QVBoxLayout *m_topLayout;
        QHBoxLayout *m_authorLayout;

        QLabel *m_label;
        QLabel *m_languageLabel;
        QLabel *m_authorLabel;

        QGroupBox *m_descriptionGroupBox;
        QVBoxLayout *m_descriptionLayout;
        QLabel *m_descriptionLabel;
    };

} // LangMgr

#endif // G2PINFOWIDGET_H
