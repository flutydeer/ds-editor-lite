#ifndef TEMPOPOPUPWIDGET_H
#define TEMPOPOPUPWIDGET_H

#include <QFrame>

class QEvent;
class QLabel;
class TempoEditWidget;

class TempoPopupWidget : public QFrame {
    Q_OBJECT

public:
    explicit TempoPopupWidget(QWidget *parent = nullptr);

    void setTempo(double tempo);
    void showAt(const QPoint &globalPos);

signals:
    void tempoSelected(double tempo);

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyWindowEffects();

    TempoEditWidget *m_editWidget = nullptr;
    QLabel *m_titleLabel = nullptr;
};

#endif // TEMPOPOPUPWIDGET_H
