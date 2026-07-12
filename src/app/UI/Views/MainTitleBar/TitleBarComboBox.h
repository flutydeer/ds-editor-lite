//
// Created by FlutyDeer on 2026/7/13.
//

#ifndef TITLEBARCOMBOBOX_H
#define TITLEBARCOMBOBOX_H

#include <QToolButton>

class FilePopupWidget;
class QMouseEvent;

class TitleBarComboBox : public QToolButton {
    Q_OBJECT

public:
    explicit TitleBarComboBox(QWidget *parent = nullptr);
    ~TitleBarComboBox() override;

    void setTitle(const QString &title);
    [[nodiscard]] FilePopupWidget *popupWidget() const;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    void showPopup();
    void hidePopup();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setPopupVisible(bool visible);

    FilePopupWidget *m_popup = nullptr;
    QString m_title;
    bool m_ignoreNextShow = false;
    bool m_hidingPopupFromCombo = false;
};

#endif // TITLEBARCOMBOBOX_H
