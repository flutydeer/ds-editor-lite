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

    bool testOwningLifetimeAndStaleReplacement() {
        bool ok = true;
        int creationCount = 0;
        int destroyedCount = 0;
        SingerSessionCache<FakeHandle> cache;
        SingerIdentifier identifier;
        identifier.packageId = QStringLiteral("package");
        identifier.singerId = QStringLiteral("singer");
        identifier.packageVersion = QVersionNumber(1, 0, 0);

        const auto createHandle = [&] {
            ++creationCount;
            return std::make_shared<FakeHandle>(destroyedCount);
        };

        std::weak_ptr<FakeHandle> firstWeak;
        {
            const auto first = cache.acquire(identifier, createHandle);
            firstWeak = first;
        }
        ok &= expect(!firstWeak.expired(), "cache retains the handle after the caller releases it");

        auto reused = cache.acquire(identifier, createHandle);
        ok &= expect(reused == firstWeak.lock(), "a non-stale handle is reused");
        ok &= expect(creationCount == 1, "reuse does not create another handle");

        reused->stale = true;
        const auto staleWeak = std::weak_ptr<FakeHandle>(reused);
        auto replacement = cache.acquire(identifier, createHandle);
        ok &= expect(replacement != reused, "a stale handle is replaced");
        ok &= expect(creationCount == 2, "stale replacement creates exactly one handle");
        reused.reset();
        ok &= expect(staleWeak.expired(), "the displaced stale handle is released");

        const auto replacementWeak = std::weak_ptr<FakeHandle>(replacement);
        cache.clear();
        ok &= expect(!replacementWeak.expired(), "an active caller survives cache clearing");
        replacement.reset();
        ok &= expect(replacementWeak.expired(),
                     "the final handle is released after its caller exits");
        ok &= expect(destroyedCount == 2, "all handles are destroyed exactly once");
        return ok;
    }
}

int main() {
    return testOwningLifetimeAndStaleReplacement() ? 0 : 1;
}
