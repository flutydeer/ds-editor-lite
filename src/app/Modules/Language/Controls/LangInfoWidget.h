#ifndef LANGINFOWIDGET_H
#define LANGINFOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <QGroupBox>
#include <QJsonObject>

namespace LangMgr {

    class LangInfoWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit LangInfoWidget(QWidget *parent = nullptr);
        ~LangInfoWidget() override;

        void setInfo(const QString &id);

    Q_SIGNALS:
        void g2pSelected(const QString &language, const QString &g2pId) const;
        void langConfigChanged() const;

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

#endif // LANGINFOWIDGET_H
