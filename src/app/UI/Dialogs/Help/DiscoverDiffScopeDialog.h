#ifndef DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H
#define DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H

#include <QDialog>

class QLabel;
class QResizeEvent;
class QWidget;

class DiscoverDiffScopeDialog final : public QDialog {
    Q_OBJECT
    Q_PROPERTY(InfoType infoType READ infoType WRITE setInfoType)

public:
    enum class InfoType {
        About,
        FeatureIntroduction,
    };
    Q_ENUM(InfoType)

    explicit DiscoverDiffScopeDialog(QWidget *parent = nullptr);
    ~DiscoverDiffScopeDialog() override;

    InfoType infoType() const;
    void setInfoType(InfoType infoType);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateBannerHeight();
    void updateTextTitle();

    QLabel *m_bannerLabel;
    QLabel *m_textLabel;
    QWidget *m_contentWidget;
    InfoType m_infoType = InfoType::About;
};

#endif // DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H
