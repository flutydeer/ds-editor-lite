#ifndef LANGINFOWIDGET_H
#define LANGINFOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

#include <QGroupBox>

namespace LangMgr {

    class LangInfoWidget final : public QWidget {
        Q_OBJECT
    public:
        explicit LangInfoWidget(QWidget *parent = nullptr);
        ~LangInfoWidget() override;

        void setInfo(const QString &id) const;

    private:
        QVBoxLayout *m_mainLayout;
        QVBoxLayout *m_topLayout;

        QLabel *m_langueLabel;
        QLabel *m_authorLabel;

        QGroupBox *m_descriptionGroupBox;
        QVBoxLayout *m_descriptionLayout;
        QLabel *m_descriptionLabel;
    };

} // LangMgr

#endif // LANGINFOWIDGET_H
