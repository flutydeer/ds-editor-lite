#include "Modules/Inference/SingerSessionCache.h"

#include <chrono>
#include <memory>

#include <QTextStream>

namespace {
    bool expect(bool condition, const char *message) {
        if (condition) {
            return true;
        }
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

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

    bool testRetainsOnlySelectedSingers() {
        FakeClock::reset();
        bool ok = true;
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
        ok &= expect(!firstWeak.expired(), "cache retains the handle after the caller releases it");

        auto reused = cache.acquire(firstIdentifier, createHandle);
        ok &= expect(reused == firstWeak.lock(), "a non-stale handle is reused");
        ok &= expect(creationCount == 1, "reuse does not create another handle");
        reused.reset();

        auto second = cache.acquire(secondIdentifier, createHandle);
        const auto secondWeak = std::weak_ptr<FakeHandle>(second);
        second.reset();

        auto releaseResult = cache.retainOnly({secondIdentifier});
        ok &= expect(releaseResult.released == 1 && releaseResult.handles.size() == 1,
                     "retention update transfers one released resident handle");
        ok &= expect(!firstWeak.expired(), "transferred handle remains alive for deferred release");
        releaseResult.handles.clear();
        ok &= expect(firstWeak.expired(), "a deselected singer handle is released");
        ok &= expect(!secondWeak.expired(), "a selected singer handle remains cached");

        auto uncached = cache.acquire(firstIdentifier, createHandle);
        const auto uncachedWeak = std::weak_ptr<FakeHandle>(uncached);
        uncached.reset();
        ok &= expect(uncachedWeak.expired(), "a deselected singer cannot re-enter the cache");

        cache.clear();
        ok &= expect(secondWeak.expired(), "clearing releases the remaining cached handle");
        ok &= expect(creationCount == 3, "only the expected handles are created");
        ok &= expect(destroyedCount == 3, "all selected and uncached handles are destroyed once");
        return ok;
    }

    bool testLatestRetainedIdentifiers() {
        FakeCache cache;
        const auto firstIdentifier = makeIdentifier(QStringLiteral("first"));
        const auto secondIdentifier = makeIdentifier(QStringLiteral("second"));

        cache.retainOnly({firstIdentifier});
        cache.retainOnly({secondIdentifier});
        cache.retainOnly({firstIdentifier});

        return expect(cache.retainedIdentifiers() == QSet{firstIdentifier},
                      "asynchronous cleanup observes the latest singer selection");
    }

    bool testActiveCallerAndStaleReplacement() {
        FakeClock::reset();
        bool ok = true;
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
        ok &= expect(replacement != active, "a stale handle is replaced");
        ok &= expect(creationCount == 2, "stale replacement creates exactly one handle");
        active.reset();
        ok &= expect(staleWeak.expired(), "the displaced stale handle is released");

        const auto replacementWeak = std::weak_ptr<FakeHandle>(replacement);
        auto releaseResult = cache.retainOnly({});
        ok &= expect(releaseResult.released == 1 && releaseResult.handles.size() == 1,
                     "retention update transfers an active deselected resident handle");
        releaseResult.handles.clear();
        ok &= expect(!replacementWeak.expired(), "an active caller survives cache eviction");
        replacement.reset();
        ok &=
            expect(replacementWeak.expired(), "an evicted handle releases after its caller exits");
        ok &= expect(destroyedCount == 2, "all handles are destroyed exactly once");
        return ok;
    }

    bool testLeastRecentlyUsedEviction() {
        FakeClock::reset();
        bool ok = true;
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
        ok &= expect(result.capacity == 0 && !firstWeak.expired() && !secondWeak.expired() &&
                         !thirdWeak.expired(),
                     "unlimited capacity preserves all resident handles");

        result = cache.evict(2, std::chrono::milliseconds::zero());
        ok &= expect(result.capacity == 1 && result.idle == 0 && result.handles.size() == 1,
                     "capacity scan transfers one LRU handle for deferred release");
        ok &= expect(!secondWeak.expired(), "transferred LRU handle remains alive until released");
        result.handles.clear();
        ok &= expect(!firstWeak.expired(), "recently reused handle remains cached");
        ok &= expect(secondWeak.expired(), "least recently used handle is released");
        ok &= expect(!thirdWeak.expired(), "newer handle remains cached");
        ok &= expect(creationCount == 3, "LRU scan does not create handles");

        cache.clear();
        ok &= expect(firstWeak.expired() && thirdWeak.expired(),
                     "clearing releases the remaining LRU entries");
        ok &= expect(destroyedCount == 3, "all LRU handles are destroyed exactly once");
        return ok;
    }

    bool testIdleEvictionAndActiveReuse() {
        FakeClock::reset();
        bool ok = true;
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
        ok &= expect(result.idle == 0 && !weak.expired(),
                     "handle remains resident before the idle timeout");

        FakeClock::advance(std::chrono::milliseconds(1));
        result = cache.evict(8, std::chrono::milliseconds(60));
        ok &= expect(result.idle == 1 && result.handles.size() == 1 && !weak.expired(),
                     "idle scan transfers the expired handle for deferred release");
        result.handles.clear();
        ok &= expect(result.idle == 1 && weak.expired(), "idle handle is released at the timeout");

        auto active = cache.acquire(identifier, createHandle);
        const auto activeWeak = std::weak_ptr<FakeHandle>(active);
        FakeClock::advance(std::chrono::milliseconds(60));
        result = cache.evict(8, std::chrono::milliseconds(60));
        ok &= expect(result.handles.size() == 1,
                     "active idle handle is transferred for deferred release");
        result.handles.clear();
        ok &= expect(result.idle == 1 && !activeWeak.expired(),
                     "an active caller survives idle eviction");
        auto reused = cache.acquire(identifier, createHandle);
        ok &= expect(reused == active, "an active evicted handle is promoted instead of recreated");
        ok &= expect(creationCount == 2, "idle promotion avoids duplicate handle creation");

        active.reset();
        reused.reset();
        cache.clear();
        ok &= expect(activeWeak.expired(), "promoted handle releases when the cache is cleared");
        ok &= expect(destroyedCount == 2, "all idle-test handles are destroyed exactly once");
        return ok;
    }
}

int main() {
    return testRetainsOnlySelectedSingers() && testLatestRetainedIdentifiers() &&
                   testActiveCallerAndStaleReplacement() && testLeastRecentlyUsedEviction() &&
                   testIdleEvictionAndActiveReuse()
               ? 0
               : 1;
}
