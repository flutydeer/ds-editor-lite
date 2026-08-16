#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <QMWidgets/ccombobox.h>

class Menu;

class ComboBox : public CComboBox, public WheelEventPolicySupport {
    Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr);
    explicit ComboBox(WheelEventPolicy wheelEventPolicy, QWidget *parent = nullptr);

    [[nodiscard]] Menu *createContextMenu(QWidget *parent = nullptr);

private:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    void initUi();
};



#endif // COMBOBOX_H
