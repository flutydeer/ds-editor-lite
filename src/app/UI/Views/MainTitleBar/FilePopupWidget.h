//
// Created by FlutyDeer on 2026/7/13.
//

#ifndef FILEPOPUPWIDGET_H
#define FILEPOPUPWIDGET_H

#include <QFrame>
#include <QList>
#include <QString>

class QVBoxLayout;
class QLabel;
class QHideEvent;

class FilePopupWidget : public QFrame {
    Q_OBJECT

public:
    explicit FilePopupWidget(QWidget *parent = nullptr);

    void showAt(const QPoint &globalPos);

signals:
    void newProjectClicked();
    void openProjectClicked();
    void openRecentProject(const QString &filePath);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void clearRecentItemHoverState();
    void refreshRecentFiles();
    QWidget *createRecentFileItem(const QString &filePath, bool isCurrent);
    void syncPopupGeometry();
    void applyWindowEffects();

    QFrame *m_surface = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QLabel *m_lbEmpty = nullptr;
    QList<QWidget *> m_recentItems;
};

#endif // FILEPOPUPWIDGET_H
