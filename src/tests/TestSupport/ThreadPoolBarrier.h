#pragma once

#include <QSemaphore>
#include <QThreadPool>

namespace TestSupport {
    class ThreadPoolBarrier final {
    public:
        ThreadPoolBarrier() : maximumThreads(pool->maxThreadCount()) {
            pool->setMaxThreadCount(1);
            pool->start([this] {
                entered.release();
                release.acquire();
            });
        }

        ~ThreadPoolBarrier() {
            resume();
            pool->waitForDone();
            pool->setMaxThreadCount(maximumThreads);
        }

        bool ready() const {
            return entered.available() == 1;
        }

        void resume() {
            release.release();
        }

    private:
        QThreadPool *pool = QThreadPool::globalInstance();
        int maximumThreads;
        QSemaphore entered;
        QSemaphore release;
    };
}
