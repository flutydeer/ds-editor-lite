#ifndef DELAYTIMER_H
#define DELAYTIMER_H

#include <QObject>
#include <QTimer>
#include <QMutexLocker>

class DelayTimer final : public QObject {
    Q_OBJECT

public:
    explicit DelayTimer(QObject *parent = nullptr) : QObject(parent), m_timer(new QTimer(this)) {
        m_timer->setSingleShot(false);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (m_decay > 0)
                this->timeoutSignal();
        });
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

    void triggerNow() {
        QMutexLocker locker(&m_mutex);
        if (m_decay > 0) {
            if (m_timer->isActive()) {
                m_timer->stop();
            }
            Q_EMIT this->timeoutSignal();
        }
    }

    bool timeout() const {
        return m_decay == 0 || (!m_timer->isActive());
    }

    bool autoStart() const {
        return !static_cast<bool>(m_decay);
    }

signals:
    void timeoutSignal();

private:
    QTimer *m_timer;
    QMutex m_mutex;
    int m_decay = 0;
};

#endif // DELAYTIMER_H
