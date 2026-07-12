#ifndef DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H
#define DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H

#include <QDialog>

class QLabel;
class QResizeEvent;
class QWidget;

class DiscoverDiffScopeDialog final : public QDialog {
    Q_OBJECT

public:
    explicit DiscoverDiffScopeDialog(QWidget *parent = nullptr);
    ~DiscoverDiffScopeDialog() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateBannerHeight();

    QLabel *m_bannerLabel;
    QWidget *m_contentWidget;
};

#endif // DS_EDITOR_LITE_DISCOVERDIFFSCOPEDIALOG_H
