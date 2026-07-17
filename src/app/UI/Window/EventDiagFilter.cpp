// ====== TEMPORARY DIAGNOSTIC EVENT FILTER (remove after investigation) ======

#include "EventDiagFilter.h"

#include <QDebug>
#include <QEvent>
#include <QWidget>

static constexpr int kDiagIntervalMs = 1000;
static constexpr int kBacklogWarnMs = 100;
static constexpr int kBacklogSevereMs = 500;

EventDiagFilter::EventDiagFilter(QObject *parent) : QObject(parent) {
    m_timer.setInterval(kDiagIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &EventDiagFilter::printStats);
    m_timer.start();
    m_lastPrintTime.start();
    m_sessionStart.start();
}

bool EventDiagFilter::eventFilter(QObject *obj, QEvent *event) {
    // measure gap since previous event before updating timestamp
    qint64 gapMs = m_lastEventTime.elapsed();
    if (gapMs > m_maxGapMs)
        m_maxGapMs = gapMs;
    if (gapMs >= kBacklogSevereMs)
        m_severeGapCount++;
    else if (gapMs >= kBacklogWarnMs)
        m_warnGapCount++;

    // track paint blocking time: accumulate gap contributed by preceding Paint
    if (m_lastEventWasPaint && gapMs > 0) {
        m_paintTimeUs += gapMs * 1000;
        if (!m_lastPaintWidgetClass.isEmpty())
            m_paintTimeByClass[m_lastPaintWidgetClass] += gapMs * 1000;
    }
    m_lastEventTime.start();

    m_lastEventWasPaint = (event->type() == QEvent::Paint);
    if (m_lastEventWasPaint) {
        if (auto widget = qobject_cast<QWidget *>(obj))
            m_lastPaintWidgetClass = QString::fromLatin1(widget->metaObject()->className());
        else
            m_lastPaintWidgetClass.clear();
    }

    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
            m_inputCount++;
            break;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            m_keyCount++;
            break;
        case QEvent::MetaCall:
            m_metaCallCount++;
            m_metaCallDepth++;
            break;
        case QEvent::Timer:
            m_timerCount++;
            break;
        case QEvent::UpdateRequest:
            m_updateReqCount++;
            break;
        case QEvent::Paint: {
            m_paintCount++;
            if (auto widget = qobject_cast<QWidget *>(obj))
                m_paintByClass[QString::fromLatin1(widget->metaObject()->className())]++;
            break;
        }
        default:
            m_otherCount++;
            break;
    }
    bool ret = QObject::eventFilter(obj, event);
    if (event->type() == QEvent::MetaCall)
        m_metaCallDepth--;
    return ret;
}

void EventDiagFilter::printStats() {
    double elapsedSec = m_lastPrintTime.restart() / 1000.0;
    if (elapsedSec <= 0)
        elapsedSec = 1.0;

    int total = m_metaCallCount + m_timerCount + m_updateReqCount + m_paintCount + m_inputCount +
                m_keyCount + m_otherCount;
    double metaCallsPerSec = m_metaCallCount / elapsedSec;
    double timersPerSec = m_timerCount / elapsedSec;
    double updateReqsPerSec = m_updateReqCount / elapsedSec;
    double paintsPerSec = m_paintCount / elapsedSec;
    double inputsPerSec = (m_inputCount + m_keyCount) / elapsedSec;
    double totalPerSec = total / elapsedSec;
    double inputRatio = total > 0 ? 100.0 * (m_inputCount + m_keyCount) / total : 0;

    // actual interval may exceed 1s if timer itself is delayed (strong backlog)
    bool intervalDelayed = elapsedSec > 1.5;

    qDebug().noquote().nospace()
        << "[Diag] interval=" << QString::number(elapsedSec, 'f', 1) << "s"
        << " | total=" << qSetFieldWidth(5) << int(totalPerSec) << qSetFieldWidth(0)
        << "/s | MetaCall=" << qSetFieldWidth(5) << int(metaCallsPerSec) << qSetFieldWidth(0)
        << "/s | Timer=" << qSetFieldWidth(5) << int(timersPerSec) << qSetFieldWidth(0)
        << "/s | Paint=" << qSetFieldWidth(4) << int(paintsPerSec) << qSetFieldWidth(0)
        << "/s | Input=" << qSetFieldWidth(3) << int(inputsPerSec) << qSetFieldWidth(0) << "/s ("
        << QString::number(inputRatio, 'f', 1) << "%)";

    qDebug().noquote().nospace()
        << "  [Backlog] maxGap=" << m_maxGapMs << "ms"
        << " | gaps>=" << kBacklogSevereMs << "ms: " << m_severeGapCount
        << " | gaps>=" << kBacklogWarnMs << "ms: " << m_warnGapCount
        << " | paintTime=" << QString::number(m_paintTimeUs / 1000.0, 'f', 0) << "ms"
        << " (" << QString::number(m_paintTimeUs / 10000.0 / elapsedSec, 'f', 1) << "% CPU)"
        << (intervalDelayed ? " | *** TIMER DELAYED ***" : "");

    // Sort paint sources by time descending, print top entries
    using Pair = QPair<QString, qint64>;
    QList<Pair> sortedByTime;
    for (auto it = m_paintTimeByClass.begin(); it != m_paintTimeByClass.end(); ++it)
        sortedByTime.append({it.key(), it.value()});
    std::sort(sortedByTime.begin(), sortedByTime.end(),
              [](const Pair &a, const Pair &b) { return a.second > b.second; });

    int shown = 0;
    qint64 accountedTime = 0;
    for (const auto &pair : sortedByTime) {
        if (shown >= 12)
            break;
        auto count = m_paintByClass.value(pair.first, 0);
        qDebug().noquote().nospace()
            << "  " << qSetFieldWidth(35) << pair.first << qSetFieldWidth(0)
            << "  count=" << qSetFieldWidth(3) << count << qSetFieldWidth(0)
            << "  time=" << qSetFieldWidth(4) << QString::number(pair.second / 1000.0, 'f', 1)
            << qSetFieldWidth(0) << "ms"
            << "  avg=" << qSetFieldWidth(4)
            << QString::number(count > 0 ? pair.second / 1000.0 / count : 0, 'f', 2)
            << qSetFieldWidth(0) << "ms";
        accountedTime += pair.second;
        shown++;
    }
    qint64 unaccountedTime = m_paintTimeUs - accountedTime;
    int unaccountedCount = m_paintTimeByClass.size() - shown;
    if (unaccountedTime > 1000 || unaccountedCount > 0) {
        qDebug().noquote().nospace() << "  ... + " << unaccountedCount << " more classes ("
                                     << QString::number(unaccountedTime / 1000.0, 'f', 1) << "ms)";
    }

    m_metaCallCount = 0;
    m_timerCount = 0;
    m_updateReqCount = 0;
    m_paintCount = 0;
    m_inputCount = 0;
    m_keyCount = 0;
    m_otherCount = 0;
    m_maxGapMs = 0;
    m_severeGapCount = 0;
    m_warnGapCount = 0;
    m_paintTimeUs = 0;
    m_paintByClass.clear();
    m_paintTimeByClass.clear();
}

// ====== END DIAGNOSTIC EVENT FILTER ======
