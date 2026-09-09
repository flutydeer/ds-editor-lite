#include <QtTest/QTest>
#include "Modules/Inference/SingerSessionCache.h"

#include <chrono>
#include <memory>

namespace {

    struct FakeHandle {
        explicit FakeHandle(int &destroyedCount) : destroyedCount(destroyedCount) {
        }

        ~FakeHandle() {
            ++destroyedCount;
        }

        bool isStale() const noexcept {
            return stale;
        }

        int &destroyedCount;
        bool stale = false;
    };

    struct FakeClock {
        using duration = std::chrono::milliseconds;
        using rep = duration::rep;
        using period = duration::period;
        using time_point = std::chrono::time_point<FakeClock>;
        static constexpr bool is_steady = true;

        static time_point now() noexcept {
            return current;
        }

        static void reset() noexcept {
            current = time_point{};
        }

        static void advance(const duration amount) noexcept {
            current += amount;
        }

        inline static time_point current{};
    };

    using FakeCache = SingerSessionCache<FakeHandle, FakeClock>;

    SingerIdentifier makeIdentifier(const QString &singerId) {
        SingerIdentifier identifier;
        identifier.packageId = QStringLiteral("package");
        identifier.singerId = singerId;
        identifier.packageVersion = QVersionNumber(1, 0, 0);
        return identifier;
    }

}

class SingerSessionCacheTests final : public QObject {
    Q_OBJECT

private slots:

    void retainsOnlySelectedSingers() {
        FakeClock::reset();
        int creationCount = 0;
        int destroyedCount = 0;
        FakeCache cache;
        const auto firstIdentifier = makeIdentifier(QStringLiteral("first"));
        const auto secondIdentifier = makeIdentifier(QStringLiteral("second"));
        cache.retainOnly({firstIdentifier, secondIdentifier});

        const auto createHandle = [&] {
            ++creationCount;
            return std::make_shared<FakeHandle>(destroyedCount);
        };

        std::weak_ptr<FakeHandle> firstWeak;
        {
            const auto first = cache.acquire(firstIdentifier, createHandle);
            firstWeak = first;
        }
        QVERIFY2(!firstWeak.expired(), "cache retains the handle after the caller releases it");

        auto reused = cache.acquire(firstIdentifier, createHandle);
        QCOMPARE(reused, firstWeak.lock());
        QCOMPARE(creationCount, 1);
        reused.reset();

        auto second = cache.acquire(secondIdentifier, createHandle);
        const auto secondWeak = std::weak_ptr<FakeHandle>(second);
        second.reset();

        auto releaseResult = cache.retainOnly({secondIdentifier});
        QCOMPARE(releaseResult.released, 1);
        QCOMPARE(releaseResult.handles.size(), 1);
        QVERIFY2(!firstWeak.expired(), "transferred handle remains alive for deferred release");
        releaseResult.handles.clear();
        QVERIFY2(firstWeak.expired(), "a deselected singer handle is released");
        QVERIFY2(!secondWeak.expired(), "a selected singer handle remains cached");

        auto uncached = cache.acquire(firstIdentifier, createHandle);
        const auto uncachedWeak = std::weak_ptr<FakeHandle>(uncached);
        uncached.reset();
        QVERIFY2(uncachedWeak.expired(), "a deselected singer cannot re-enter the cache");

        cache.clear();
        QVERIFY2(secondWeak.expired(), "clearing releases the remaining cached handle");
        QCOMPARE(creationCount, 3);
        QCOMPARE(destroyedCount, 3);
    }

    void latestRetainedIdentifiers() {
        FakeCache cache;
        const auto firstIdentifier = makeIdentifier(QStringLiteral("first"));
        const auto secondIdentifier = makeIdentifier(QStringLiteral("second"));

        cache.retainOnly({firstIdentifier});
        cache.retainOnly({secondIdentifier});
        cache.retainOnly({firstIdentifier});

        QCOMPARE(cache.retainedIdentifiers(), QSet{firstIdentifier});
    }

    void activeCallerAndStaleReplacement() {
        FakeClock::reset();
        int creationCount = 0;
        int destroyedCount = 0;
        FakeCache cache;
        const auto identifier = makeIdentifier(QStringLiteral("singer"));
        cache.retainOnly({identifier});

        const auto createHandle = [&] {
            ++creationCount;
            return std::make_shared<FakeHandle>(destroyedCount);
        };

        auto active = cache.acquire(identifier, createHandle);
        active->stale = true;
        const auto staleWeak = std::weak_ptr<FakeHandle>(active);
        auto replacement = cache.acquire(identifier, createHandle);
        QVERIFY2(replacement != active, "a stale handle is replaced");
        QCOMPARE(creationCount, 2);
        active.reset();
        QVERIFY2(staleWeak.expired(), "the displaced stale handle is released");

        const auto replacementWeak = std::weak_ptr<FakeHandle>(replacement);
        auto releaseResult = cache.retainOnly({});
        QCOMPARE(releaseResult.released, 1);
        QCOMPARE(releaseResult.handles.size(), 1);
        releaseResult.handles.clear();
        QVERIFY2(!replacementWeak.expired(), "an active caller survives cache eviction");
        replacement.reset();
        QVERIFY2(replacementWeak.expired(), "an evicted handle releases after its caller exits");
        QCOMPARE(destroyedCount, 2);
    }

    void leastRecentlyUsedEviction() {
        FakeClock::reset();
        int creationCount = 0;
        int destroyedCount = 0;
        FakeCache cache;
        const auto firstIdentifier = makeIdentifier(QStringLiteral("first"));
        const auto secondIdentifier = makeIdentifier(QStringLiteral("second"));
        const auto thirdIdentifier = makeIdentifier(QStringLiteral("third"));
        cache.retainOnly({firstIdentifier, secondIdentifier, thirdIdentifier});

        const auto createHandle = [&] {
            ++creationCount;
            return std::make_shared<FakeHandle>(destroyedCount);
        };

        auto first = cache.acquire(firstIdentifier, createHandle);
        const auto firstWeak = std::weak_ptr<FakeHandle>(first);
        first.reset();
        FakeClock::advance(std::chrono::milliseconds(10));
        auto second = cache.acquire(secondIdentifier, createHandle);
        const auto secondWeak = std::weak_ptr<FakeHandle>(second);
        second.reset();
        FakeClock::advance(std::chrono::milliseconds(10));
        auto third = cache.acquire(thirdIdentifier, createHandle);
        const auto thirdWeak = std::weak_ptr<FakeHandle>(third);
        third.reset();
        FakeClock::advance(std::chrono::milliseconds(10));
        auto touched = cache.acquire(firstIdentifier, createHandle);
        touched.reset();

        auto result = cache.evict(0, std::chrono::milliseconds::zero());
        QCOMPARE(result.capacity, 0);
        QVERIFY2(!firstWeak.expired(), "unlimited capacity preserves all resident handles");
        QVERIFY2(!secondWeak.expired(), "unlimited capacity preserves all resident handles");
        QVERIFY2(!thirdWeak.expired(), "unlimited capacity preserves all resident handles");

        result = cache.evict(2, std::chrono::milliseconds::zero());
        QCOMPARE(result.capacity, 1);
        QCOMPARE(result.idle, 0);
        QCOMPARE(result.handles.size(), 1);
        QVERIFY2(!secondWeak.expired(), "transferred LRU handle remains alive until released");
        result.handles.clear();
        QVERIFY2(!firstWeak.expired(), "recently reused handle remains cached");
        QVERIFY2(secondWeak.expired(), "least recently used handle is released");
        QVERIFY2(!thirdWeak.expired(), "newer handle remains cached");
        QCOMPARE(creationCount, 3);

        cache.clear();
        QVERIFY2(firstWeak.expired(), "clearing releases the remaining LRU entries");
        QVERIFY2(thirdWeak.expired(), "clearing releases the remaining LRU entries");
        QCOMPARE(destroyedCount, 3);
    }

    void idleEvictionAndActiveReuse() {
        FakeClock::reset();
        int creationCount = 0;
        int destroyedCount = 0;
        FakeCache cache;
        const auto identifier = makeIdentifier(QStringLiteral("singer"));
        cache.retainOnly({identifier});

        const auto createHandle = [&] {
            ++creationCount;
            return std::make_shared<FakeHandle>(destroyedCount);
        };

        auto handle = cache.acquire(identifier, createHandle);
        const auto weak = std::weak_ptr<FakeHandle>(handle);
        handle.reset();
        FakeClock::advance(std::chrono::milliseconds(59));
        auto result = cache.evict(8, std::chrono::milliseconds(60));
        QCOMPARE(result.idle, 0);
        QVERIFY2(!weak.expired(), "handle remains resident before the idle timeout");

        FakeClock::advance(std::chrono::milliseconds(1));
        result = cache.evict(8, std::chrono::milliseconds(60));
        QCOMPARE(result.idle, 1);
        QCOMPARE(result.handles.size(), 1);
        QVERIFY2(!weak.expired(), "idle scan transfers the expired handle for deferred release");
        result.handles.clear();
        QCOMPARE(result.idle, 1);
        QVERIFY2(weak.expired(), "idle handle is released at the timeout");

        auto active = cache.acquire(identifier, createHandle);
        const auto activeWeak = std::weak_ptr<FakeHandle>(active);
        FakeClock::advance(std::chrono::milliseconds(60));
        result = cache.evict(8, std::chrono::milliseconds(60));
        QCOMPARE(result.handles.size(), 1);
        result.handles.clear();
        QCOMPARE(result.idle, 1);
        QVERIFY2(!activeWeak.expired(), "an active caller survives idle eviction");
        auto reused = cache.acquire(identifier, createHandle);
        QCOMPARE(reused, active);
        QCOMPARE(creationCount, 2);

        active.reset();
        reused.reset();
        cache.clear();
        QVERIFY2(activeWeak.expired(), "promoted handle releases when the cache is cleared");
        QCOMPARE(destroyedCount, 2);
    }
};

QTEST_APPLESS_MAIN(SingerSessionCacheTests)
#include "main.moc"
