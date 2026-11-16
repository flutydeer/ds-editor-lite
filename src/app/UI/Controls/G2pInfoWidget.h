#ifndef G2PINFOWIDGET_H
#define G2PINFOWIDGET_H

#include <QVBoxLayout>
#include <QLabel>
#include <QVersionNumber>

#include <QGroupBox>

namespace LangSetting {

    class G2pInfoWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit G2pInfoWidget(QWidget *parent = nullptr);
        ~G2pInfoWidget() override;

    public Q_SLOTS:
        // Use the official context. An empty id displays the unselected state.
        void setInfo(const QString &g2pId) const;
        // Select a voicebank context and version. Empty context and null version select the
        // official context.
        void setInfo(const QString &g2pId, const QString &context,
                     const QVersionNumber &contextVersion) const;

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
