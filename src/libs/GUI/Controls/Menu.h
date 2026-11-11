#ifndef MENU_H
#define MENU_H

#include <QMWidgets/cmenu.h>

#include <QColor>

class QLineEdit;

class Menu : public CMenu {
    Q_OBJECT
    Q_PROPERTY(QColor shortcutTextColor READ shortcutTextColor WRITE setShortcutTextColor)
    Q_PROPERTY(QColor shortcutSelectedTextColor READ shortcutSelectedTextColor WRITE
                   setShortcutSelectedTextColor)
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);
    static Menu *fromLineEdit(QLineEdit *editor, QWidget *parent = nullptr);

    QColor shortcutTextColor() const;
    void setShortcutTextColor(const QColor &color);
    QColor shortcutSelectedTextColor() const;
    void setShortcutSelectedTextColor(const QColor &color);

protected:
    bool event(QEvent *event) override;

private:
    void paintEvent(QPaintEvent *event) override;
    void initUi();
    void updateSubMenuArrowSpace();

    QColor m_shortcutTextColor;
    QColor m_shortcutSelectedTextColor;
};

#endif // MENU_H
