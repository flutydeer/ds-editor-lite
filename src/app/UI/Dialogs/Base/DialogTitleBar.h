//
// Created on 2026/4/23.
//

#ifndef DIALOGTITLEBAR_H
#define DIALOGTITLEBAR_H

#include <QWidget>

class QVariantAnimation;
class QGraphicsOpacityEffect;
class QLabel;
class Button;

class DialogTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit DialogTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] Button *closeButton() const;

    void setTitle(const QString &title) const;

signals:
    void closeTriggered();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveStyle(bool active) const;

    QWidget *m_window;
    QLabel *m_lbTitle = nullptr;
    Button *m_btnClose = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
};

#endif // DIALOGTITLEBAR_H
