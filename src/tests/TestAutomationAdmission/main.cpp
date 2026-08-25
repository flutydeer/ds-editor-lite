#include "Automation/Public/AdmissionController.h"

#include <QCoreApplication>
#include <QList>
#include <QTextStream>

#include <utility>

namespace {
    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    bool hasError(const Automation::AutomationResult<Automation::AdmissionLease> &result,
                  const Automation::AutomationErrorCode code) {
        return !result && result.getError().code == code;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);

    auto defaultLimits = Automation::AdmissionLimits{};
    expect(defaultLimits.maximumClientInFlight == 32 && defaultLimits.tokenCapacity >= 64.0,
           QStringLiteral("default admission must reserve tokens for a 32-request burst"));
    defaultLimits.maximumGlobalInFlight = 33;
    Automation::AdmissionController defaultController(defaultLimits);
    for (int index = 0; index < 8; ++index) {
        const auto preliminary = defaultController.tryAcquire(QStringLiteral("default-client"));
        expect(bool(preliminary),
               QStringLiteral("preliminary requests must not consume the 32-request burst"));
    }
    QList<Automation::AdmissionLease> defaultLeases;
    for (int index = 0; index < 32; ++index) {
        auto result = defaultController.tryAcquire(QStringLiteral("default-client"));
        expect(bool(result), QStringLiteral("each request within the 32-request limit must pass"));
        if (result)
            defaultLeases.append(std::move(result.get()));
    }
    const auto overDefaultLimit =
        defaultController.tryAcquire(QStringLiteral("default-client"));
    expect(hasError(overDefaultLimit, Automation::AutomationErrorCode::TooManyRequests),
           QStringLiteral("the thirty-third request from one client must be rejected"));

    qint64 now = 1000;
    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = 2;
    limits.maximumClientInFlight = 1;
    limits.maximumBackgroundTasks = 1;
    limits.maximumPerDomain = 1;
    limits.tokenCapacity = 10.0;
    limits.tokensPerSecond = 1.0;
    Automation::AdmissionController controller(limits, [&now] { return now; });

    auto firstResult = controller.tryAcquire(QStringLiteral("client-a"),
                                             QStringLiteral("document-write"));
    expect(firstResult && firstResult.get().isValid(),
           QStringLiteral("first request must acquire an admission lease"));
    auto firstLease = firstResult ? std::move(firstResult.get()) : Automation::AdmissionLease{};

    const auto sameClient = controller.tryAcquire(QStringLiteral("client-a"),
                                                  QStringLiteral("playback"));
    expect(hasError(sameClient, Automation::AutomationErrorCode::TooManyRequests),
           QStringLiteral("per-client in-flight limit must be stable too_many_requests"));

    const auto sameDomain = controller.tryAcquire(QStringLiteral("client-b"),
                                                  QStringLiteral("document-write"));
    expect(hasError(sameDomain, Automation::AutomationErrorCode::Busy),
           QStringLiteral("occupied concurrency domain must be reported as busy"));

    auto backgroundResult = controller.tryAcquire(QStringLiteral("client-b"),
                                                  QStringLiteral("render"), true);
    expect(backgroundResult && backgroundResult.get().isValid(),
           QStringLiteral("independent client and domain must acquire remaining capacity"));
    auto backgroundLease = backgroundResult ? std::move(backgroundResult.get())
                                            : Automation::AdmissionLease{};

    const auto globalLimit = controller.tryAcquire(QStringLiteral("client-c"));
    expect(hasError(globalLimit, Automation::AutomationErrorCode::Busy),
           QStringLiteral("global in-flight limit must be reported as busy"));

    const auto full = controller.snapshot();
    expect(full.globalInFlight == 2 && full.backgroundTasks == 1 &&
               full.clientInFlight.value(QStringLiteral("client-a")) == 1 &&
               full.domainInFlight.value(QStringLiteral("render")) == 1,
           QStringLiteral("snapshot must report live client/domain/background counts"));

    firstLease = {};
    backgroundLease = {};
    const auto released = controller.snapshot();
    expect(released.globalInFlight == 0 && released.backgroundTasks == 0 &&
               released.clientInFlight.isEmpty() && released.domainInFlight.isEmpty(),
           QStringLiteral("releasing the last lease copy must release all counters"));

    controller.setAccepting(false);
    const auto stopping = controller.tryAcquire(QStringLiteral("client-a"));
    expect(hasError(stopping, Automation::AutomationErrorCode::OperationUnavailable) &&
               !controller.snapshot().accepting,
           QStringLiteral("stopping admission must reject new requests without changing counters"));

    qint64 rateNow = 5000;
    Automation::AdmissionLimits rateLimits;
    rateLimits.maximumGlobalInFlight = 4;
    rateLimits.maximumClientInFlight = 4;
    rateLimits.maximumPerDomain = 4;
    rateLimits.tokenCapacity = 2.0;
    rateLimits.tokensPerSecond = 1.0;
    Automation::AdmissionController rateController(rateLimits, [&rateNow] { return rateNow; });

    {
        auto request = rateController.tryAcquire(QStringLiteral("rate-client"));
        expect(bool(request), QStringLiteral("first token must be available"));
    }
    {
        auto request = rateController.tryAcquire(QStringLiteral("rate-client"));
        expect(bool(request), QStringLiteral("second token must be available"));
    }
    const auto exhausted = rateController.tryAcquire(QStringLiteral("rate-client"));
    expect(hasError(exhausted, Automation::AutomationErrorCode::TooManyRequests),
           QStringLiteral("empty token bucket must reject the request"));
    rateNow += 1000;
    const auto refilled = rateController.tryAcquire(QStringLiteral("rate-client"));
    expect(bool(refilled), QStringLiteral("fake clock must deterministically refill one token"));

    Automation::AdmissionLimits backgroundLimits;
    backgroundLimits.maximumGlobalInFlight = 4;
    backgroundLimits.maximumClientInFlight = 4;
    backgroundLimits.maximumBackgroundTasks = 1;
    backgroundLimits.maximumPerDomain = 4;
    backgroundLimits.tokenCapacity = 10.0;
    Automation::AdmissionController backgroundController(backgroundLimits);
    const auto firstBackground = backgroundController.tryAcquire(
        QStringLiteral("background-a"), QStringLiteral("render-a"), true);
    const auto secondBackground = backgroundController.tryAcquire(
        QStringLiteral("background-b"), QStringLiteral("render-b"), true);
    expect(firstBackground &&
               hasError(secondBackground, Automation::AutomationErrorCode::Busy),
           QStringLiteral("background task limit must be independent from request concurrency"));

    return failures == 0 ? 0 : 1;
}
