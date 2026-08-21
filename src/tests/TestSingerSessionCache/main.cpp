#include "Modules/Inference/SingerSessionCache.h"

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

    SingerIdentifier makeIdentifier(const QString &singerId) {
        SingerIdentifier identifier;
        identifier.packageId = QStringLiteral("package");
        identifier.singerId = singerId;
        identifier.packageVersion = QVersionNumber(1, 0, 0);
        return identifier;
    }

    bool testRetainsOnlySelectedSingers() {
        bool ok = true;
        int creationCount = 0;
        int destroyedCount = 0;
        SingerSessionCache<FakeHandle> cache;
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

        cache.retainOnly({secondIdentifier});
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

    bool testActiveCallerAndStaleReplacement() {
        bool ok = true;
        int creationCount = 0;
        int destroyedCount = 0;
        SingerSessionCache<FakeHandle> cache;
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
        cache.retainOnly({});
        ok &= expect(!replacementWeak.expired(), "an active caller survives cache eviction");
        replacement.reset();
        ok &= expect(replacementWeak.expired(),
                     "an evicted handle releases after its caller exits");
        ok &= expect(destroyedCount == 2, "all handles are destroyed exactly once");
        return ok;
    }
}

int main() {
    return testRetainsOnlySelectedSingers() && testActiveCallerAndStaleReplacement() ? 0 : 1;
}
