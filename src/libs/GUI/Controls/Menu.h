#ifndef MENU_H
#define MENU_H

#include <QMWidgets/cmenu.h>

class QLineEdit;

class Menu : public CMenu {
    Q_OBJECT
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);
    static Menu *fromLineEdit(QLineEdit *editor, QWidget *parent = nullptr);

private:
    void paintEvent(QPaintEvent *event) override;
    void initUi();
};

#endif // MENU_H
