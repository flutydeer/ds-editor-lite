#include "tst_foundation.h"

#include <lite/Core/SingletonRegistry.h>
#include <lite/Support/ObjectContext.h>

#include <QtTest>

#include <atomic>
#include <thread>

namespace {
    struct StableService {};

    template <size_t Index>
    struct AdditionalService {};

    template <size_t... Index>
    void registerServices(std::index_sequence<Index...>) {
        (SingletonRegistry::create<AdditionalService<Index>>(), ...);
    }

    template <size_t... Index>
    void destroyServices(std::index_sequence<Index...>) {
        (SingletonRegistry::destroy(SingletonRegistry::instance<AdditionalService<Index>>()), ...);
    }
}

void FoundationTests::registeredServicesRemainVisibleDuringConcurrentRegistration() {
    StableService service;
    SingletonRegistry::add(&service);
    std::atomic_bool readerStarted = false;
    std::atomic_bool registrationFinished = false;
    std::atomic_bool lookupFailed = false;
    std::thread reader([&] {
        readerStarted = true;
        do {
            if (SingletonRegistry::instance<StableService>() != &service)
                lookupFailed = true;
        } while (!registrationFinished);
    });
    while (!readerStarted)
        std::this_thread::yield();
    // Application startup registers controllers while inference workers resolve services.
    registerServices(std::make_index_sequence<256>{});
    registrationFinished = true;
    reader.join();
    destroyServices(std::make_index_sequence<256>{});
    SingletonRegistry::remove<StableService>();
    QVERIFY(!lookupFailed);
    QVERIFY(!SingletonRegistry::instance<StableService>());
}

void FoundationTests::sharedSignalUsesTheReceiversDocument() {
    StableService first, second;
    QObject sender, receiver;
    const auto a = SingletonRegistry::createContext();
    const auto b = SingletonRegistry::createContext();
    StableService *observed = nullptr;
    {
        const SingletonRegistry::Scope scope(a);
        SingletonRegistry::add(&first);
        ObjectContext::connect(&sender, &QObject::objectNameChanged, &receiver,
                               [&] { observed = SingletonRegistry::instance<StableService>(); });
    }
    {
        const SingletonRegistry::Scope scope(b);
        SingletonRegistry::add(&second);
        sender.setObjectName("from another document");
        QCOMPARE(observed, &first);
        QCOMPARE(SingletonRegistry::instance<StableService>(), &second);
    }
}
