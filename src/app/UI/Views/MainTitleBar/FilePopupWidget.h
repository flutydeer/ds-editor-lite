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
class Button;

class FilePopupWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)

public:
    explicit FilePopupWidget(QWidget *parent = nullptr);

    void showAt(const QPoint &globalPos);

signals:
    void newProjectClicked();
    void openProjectClicked();
    void openRecentProject(const QString &filePath);

protected:
    void changeEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void clearRecentItemHoverState();
    void refreshRecentFiles();
    QWidget *createRecentFileItem(const QString &filePath, bool isCurrent);
    void syncPopupGeometry();
    void applyWindowEffects();
    void rebuildActionIcons();

    // Theme color accessors (QSS-overridable via qproperty-*); the setter
    // rebuilds recent items so their tinted icons pick up the new color
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);

    QColor m_iconColor = {0xC8, 0xC9, 0xCC};

    QFrame *m_surface = nullptr;
    QFrame *m_recentSection = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    Button *m_btnNew = nullptr;
    Button *m_btnOpen = nullptr;
    QLabel *m_lbRecentTitle = nullptr;
    QLabel *m_lbEmpty = nullptr;
    QList<QWidget *> m_recentItems;
};

#endif // FILEPOPUPWIDGET_H
