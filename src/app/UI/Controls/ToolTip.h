//
// Created by fluty on 2023/8/30.
//

#ifndef DATASET_TOOLS_TOOLTIP_H
#define DATASET_TOOLS_TOOLTIP_H

#include <QFrame>
#include <QTimer>

class QLabel;
class QVBoxLayout;
class QPropertyAnimation;

class ToolTip : public QFrame {
    Q_OBJECT

public:
    //    enum ToolTipPosition  {
    //        TOP,
    //        BOTTOM,
    //        LEFT,
    //        RIGHT,
    //        TOP_LEFT,
    //        TOP_RIGHT,
    //        BOTTOM_LEFT,
    //        BOTTOM_RIGHT
    //    };

    explicit ToolTip(QString title = "", QWidget *parent = nullptr);
    ~ToolTip();

    QString title() const;
    void setTitle(const QString &text);
    QString shortcutKey() const;
    void setShortcutKey(const QString &text);
    QList<QString> message() const;
    void setMessage(const QList<QString> &text);
    void appendMessage(const QString &text);
    void clearMessage();

protected:
    QString m_title;
    QString m_shortcutKey;
    QList<QString> m_message;
    QFrame m_container;
    QTimer m_timer;

    QLabel *m_lbTitle;
    QLabel *m_lbShortcutKey;
    QVBoxLayout *m_cardLayout;
    QVBoxLayout *m_messageLayout;
    //    QTextEdit *m_teMessage;

    void setQss();
    void updateMessage();
};

#endif // DATASET_TOOLS_TOOLTIP_H
