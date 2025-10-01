//
// Created by fluty on 24-2-19.
//

#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class Button;

class DialogHeader : public QWidget {
public:
    explicit DialogHeader(QWidget *parent = nullptr);
    ~DialogHeader() override;

    void setTitle(const QString &title) const;
    void setMessage(const QString &msg) const;

private:
    QLabel *m_lbTitle;
    QLabel *m_lbMessage;
    QVBoxLayout *m_mainLayout;
};

class DialogButtonBar : public QWidget {
    Q_OBJECT

public:
    explicit DialogButtonBar(QWidget *parent = nullptr);
    ~DialogButtonBar() override;
    // void addButtonToStart(Button *button);
    void addButton(Button *button) const;
    void reset() const;

private:
    QHBoxLayout *m_mainLayout;
};

class Dialog : public QDialog {
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~Dialog() override;

    void setTitle(const QString &title) const;
    void setMessage(const QString &msg) const;
    void setPositiveButton(Button *button);
    void setNegativeButton(Button *button);
    void setNeutralButton(Button *button);

    QWidget *body() const;
    DialogButtonBar *buttonBar() const;

    static QWidget *globalParent();
    static void setGlobalContext(QWidget *parent);

private:
    using QDialog::setLayout;

    static QWidget *m_globalParent;
    QVBoxLayout *m_mainLayout;
    DialogHeader *m_header;
    QWidget *m_body = nullptr;
    DialogButtonBar *m_buttonBar = nullptr;
    Button *m_positiveButton = nullptr;
    Button *m_negativeButton = nullptr;
    Button *m_neutralButton = nullptr;

    void createButtonBar();
    void setButton();
};

#endif // DIALOG_H
