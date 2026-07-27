#ifndef TEMPOEDITWIDGET_H
#define TEMPOEDITWIDGET_H

#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include <QWidget>

class QEvent;
class TapTempoButton;

namespace SVS {
    class ExpressionDoubleSpinBox;
}

// Shared tempo value editor used by both the title-bar popup and the modal
// editor for tempo-map points. Tap Tempo only previews a measured BPM; the
// spin box remains the authoritative value that is committed by the caller.
class TempoEditWidget final : public QWidget {
    Q_OBJECT

public:
    explicit TempoEditWidget(QWidget *parent = nullptr);

    void setTempo(double tempo);
    [[nodiscard]] double tempo() const;
    void resetTapTempo();

signals:
    void tempoChanged(double tempo);

protected:
    void changeEvent(QEvent *event) override;

private:
    void recordTap();
    void expireTapTempo();

    SVS::ExpressionDoubleSpinBox *m_spinTempo = nullptr;
    TapTempoButton *m_btnTapTempo = nullptr;
    QElapsedTimer m_tapTimer;
    QList<qint64> m_tapIntervals;
    QTimer m_tapResetTimer;
    int m_displayedTapBpm = 0;
    bool m_hasDisplayedTapBpm = false;
};

#endif // TEMPOEDITWIDGET_H
