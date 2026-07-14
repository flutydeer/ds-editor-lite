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
        // 保持原签名，等价于走默认（官方）context
        // G2pPage 等全局设置页继续调用此重载，行为与改造前一致
        void setInfo(const QString &g2pId) const;
        // 带声库路由信息，供未来声库详情/调试页使用
        // context 为空串 + version 为 null 时与单参版本等价
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
