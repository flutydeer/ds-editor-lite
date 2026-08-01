#ifndef TIMESIGNATUREPOPUPWIDGET_H
#define TIMESIGNATUREPOPUPWIDGET_H

#include <QFrame>

class QLabel;
class TimeSignatureEditWidget;

class TimeSignaturePopupWidget : public QFrame {
    Q_OBJECT

public:
    explicit TimeSignaturePopupWidget(QWidget *parent = nullptr);

    void setTimeSignature(int numerator, int denominator);
    void showAt(const QPoint &globalPos);

signals:
    void timeSignatureSelected(int numerator, int denominator);

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyWindowEffects();

    TimeSignatureEditWidget *m_editWidget = nullptr;
    QLabel *m_titleLabel = nullptr;
};

#endif // TIMESIGNATUREPOPUPWIDGET_H
