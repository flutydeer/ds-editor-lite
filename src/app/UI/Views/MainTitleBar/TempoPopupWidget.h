#ifndef TEMPOPOPUPWIDGET_H
#define TEMPOPOPUPWIDGET_H

#include <QElapsedTimer>
#include <QFrame>
#include <QList>

class Button;
class QEvent;

namespace SVS {
    class ExpressionDoubleSpinBox;
}

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
    void applyEditorGeometry();
    void applyWindowEffects();
    void recordTap();

    SVS::ExpressionDoubleSpinBox *m_spinTempo = nullptr;
    Button *m_btnTapTempo = nullptr;
    QElapsedTimer m_tapTimer;
    QList<qint64> m_tapTimes;
};

#endif // TEMPOPOPUPWIDGET_H
