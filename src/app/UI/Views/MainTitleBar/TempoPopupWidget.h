#ifndef TEMPOPOPUPWIDGET_H
#define TEMPOPOPUPWIDGET_H

#include <QElapsedTimer>
#include <QFrame>
#include <QList>
#include <QTimer>

class QEvent;
class TapTempoButton;

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
    void resetTapTempo();
    void expireTapTempo();

    SVS::ExpressionDoubleSpinBox *m_spinTempo = nullptr;
    TapTempoButton *m_btnTapTempo = nullptr;
    QElapsedTimer m_tapTimer;
    QList<qint64> m_tapIntervals;
    QTimer m_tapResetTimer;
    int m_displayedTapBpm = 0;
    bool m_hasDisplayedTapBpm = false;
};

#endif // TEMPOPOPUPWIDGET_H
