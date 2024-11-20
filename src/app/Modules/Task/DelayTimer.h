#ifndef DELAYTIMER_H
#define DELAYTIMER_H

#include <QObject>
#include <QTimer>
#include <QMutexLocker>

class DelayTimer final : public QObject {
    Q_OBJECT

public:
    explicit DelayTimer(QObject *parent = nullptr) : QObject(parent), m_timer(new QTimer(this)) {
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &DelayTimer::timeoutSignal);
    }

    int decay() const {
        return m_decay;
    }

    void start(const int delayMs) {
        QMutexLocker locker(&m_mutex);
        m_timer->start(delayMs);
    }

    void reset(const int delayS = -1) {
        QMutexLocker locker(&m_mutex);
        if (m_timer->isActive()) {
            m_timer->stop();
        }

        if (delayS == 0) {
            m_decay = 0;
            return;
        }

        if (delayS == -1)
            m_timer->start(m_decay);
        else {
            m_decay = delayS * 1000;
            m_timer->start(m_decay);
        }
    }

    void cancel() {
        QMutexLocker locker(&m_mutex);
        if (m_timer->isActive()) {
            m_timer->stop();
        }
    }

    void triggerNow() {
        QMutexLocker locker(&m_mutex);
        if (m_timer->isActive()) {
            m_timer->stop();
        }
    }

    bool timeout() const {
        return !m_timer->isActive();
    }

signals:
    void timeoutSignal();

private:
    QTimer *m_timer;
    QMutex m_mutex;
    int m_decay = 0;
};

#endif // DELAYTIMER_H
