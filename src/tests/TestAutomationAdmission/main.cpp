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
    expect(defaultLimits.maximumGlobalInFlight == 32,
           QStringLiteral("default admission must allow 32 in-flight requests"));
    Automation::AdmissionController defaultController(defaultLimits);
    QList<Automation::AdmissionLease> defaultLeases;
    for (int index = 0; index < 32; ++index) {
        auto result = defaultController.tryAcquire();
        expect(bool(result), QStringLiteral("each request within the 32-request limit must pass"));
        if (result)
            defaultLeases.append(std::move(result.get()));
    }
    const auto overDefaultLimit = defaultController.tryAcquire();
    expect(hasError(overDefaultLimit, Automation::AutomationErrorCode::Busy),
           QStringLiteral("the thirty-third in-flight request must be rejected"));

    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = 2;
    limits.maximumBackgroundTasks = 1;
    Automation::AdmissionController controller(limits);

    auto firstResult = controller.tryAcquire();
    expect(firstResult && firstResult.get().isValid(),
           QStringLiteral("first request must acquire an admission lease"));
    auto firstLease = firstResult ? std::move(firstResult.get()) : Automation::AdmissionLease{};

    auto backgroundResult = controller.tryAcquire(true);
    expect(backgroundResult && backgroundResult.get().isValid(),
           QStringLiteral("a background task must acquire remaining global capacity"));
    auto backgroundLease =
        backgroundResult ? std::move(backgroundResult.get()) : Automation::AdmissionLease{};

    const auto globalLimit = controller.tryAcquire();
    expect(hasError(globalLimit, Automation::AutomationErrorCode::Busy),
           QStringLiteral("global in-flight limit must be reported as busy"));

    const auto full = controller.snapshot();
    expect(full.globalInFlight == 2 && full.backgroundTasks == 1,
           QStringLiteral("snapshot must report live request and background counts"));

    firstLease = {};
    backgroundLease = {};
    const auto released = controller.snapshot();
    expect(released.globalInFlight == 0 && released.backgroundTasks == 0,
           QStringLiteral("releasing the last lease copy must release all counters"));

    controller.setAccepting(false);
    const auto stopping = controller.tryAcquire();
    expect(hasError(stopping, Automation::AutomationErrorCode::OperationUnavailable) &&
               !controller.snapshot().accepting,
           QStringLiteral("stopping admission must reject new requests without changing counters"));

    Automation::AdmissionLimits backgroundLimits;
    backgroundLimits.maximumGlobalInFlight = 4;
    backgroundLimits.maximumBackgroundTasks = 1;
    Automation::AdmissionController backgroundController(backgroundLimits);
    const auto firstBackground = backgroundController.tryAcquire(true);
    const auto secondBackground = backgroundController.tryAcquire(true);
    expect(firstBackground && hasError(secondBackground, Automation::AutomationErrorCode::Busy),
           QStringLiteral("background task limit must be independent from request concurrency"));

    return failures == 0 ? 0 : 1;
}
