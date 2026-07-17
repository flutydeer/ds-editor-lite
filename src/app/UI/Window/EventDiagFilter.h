// ====== TEMPORARY DIAGNOSTIC EVENT FILTER (remove after investigation) ======

#ifndef EVENTDIAGFILTER_H
#define EVENTDIAGFILTER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

// App-wide event filter that tracks event throughput, paint time per widget
// class and event-loop backlog gaps; prints stats every second.
// Enabled by the developer option "enableDiagnostics".
class EventDiagFilter : public QObject {
public:
    explicit EventDiagFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void printStats();

    QTimer m_timer;
    QElapsedTimer m_lastPrintTime;
    QElapsedTimer m_lastEventTime;
    QElapsedTimer m_sessionStart;
    int m_metaCallCount = 0;
    int m_timerCount = 0;
    int m_updateReqCount = 0;
    int m_paintCount = 0;
    int m_inputCount = 0;
    int m_keyCount = 0;
    int m_otherCount = 0;
    int m_metaCallDepth = 0;
    qint64 m_maxGapMs = 0;
    int m_severeGapCount = 0;
    int m_warnGapCount = 0;
    qint64 m_paintTimeUs = 0;
    bool m_lastEventWasPaint = false;
    QString m_lastPaintWidgetClass;
    QHash<QString, int> m_paintByClass;
    QHash<QString, qint64> m_paintTimeByClass;
};

#endif // EVENTDIAGFILTER_H
