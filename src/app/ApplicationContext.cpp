#include "ApplicationContext.h"

#include "AppContext.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/DeviceTester.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/ProjectFormats/ProjectFormatRegistry.h"

#include <lite/Core/SingletonRegistry.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

struct AudioSystemContext {
    AudioSystemContext() {
        AudioSystem::outputSystem()->initialize();
        AudioSystem::midiSystem()->initialize();
        new DeviceTester(&audioSystem);
    }

    AudioSystem audioSystem;
};

#if defined(WITH_DIRECT_MANIPULATION)
struct DirectManipulationHolder {
    QWDMH::DirectManipulationSystem system;
};
#endif

ApplicationContext::ApplicationContext(std::unique_ptr<AppOptions> options) {
    AppContext::setApplication(this);

    m_appOptions = options.release();
    SingletonRegistry::add(m_appOptions);
    TaskManager::instance()->setDocumentIdProvider([] { return AppContext::currentDocumentId(); });

    m_packageManager = SingletonRegistry::create<PackageManager>();
    m_projectFormatRegistry = SingletonRegistry::create<ProjectFormatRegistry>();
    m_synthrtEngine = SingletonRegistry::create<SynthrtEngine>();
    m_inferEngine = SingletonRegistry::create<InferEngine>();
    m_inferEngine->startInitialization();
    m_audioSystem = std::make_unique<AudioSystemContext>();

#if defined(WITH_DIRECT_MANIPULATION)
    m_directManip = std::make_unique<DirectManipulationHolder>();
#endif
}

ApplicationContext::~ApplicationContext() {
    TaskManager::instance()->setDocumentIdProvider({});
    const auto appThread = QCoreApplication::instance()->thread();
    const auto drainTasks = [appThread] {
        auto *tasks = TaskManager::instance();
        tasks->terminateAllTasks();
        tasks->wait();
        if (tasks->thread() != appThread)
            tasks->moveToThread(appThread);
    };
    const auto taskThread = TaskManager::instance()->thread();
    if (taskThread == QThread::currentThread() || !taskThread->isRunning())
        drainTasks();
    else
        QMetaObject::invokeMethod(TaskManager::instance(), drainTasks,
                                  Qt::BlockingQueuedConnection);

#if defined(WITH_DIRECT_MANIPULATION)
    m_directManip.reset();
#endif
    m_audioSystem.reset();
    SingletonRegistry::destroy(m_inferEngine);
    SingletonRegistry::destroy(m_synthrtEngine);
    SingletonRegistry::destroy(m_projectFormatRegistry);
    SingletonRegistry::destroy(m_packageManager);
    SingletonRegistry::destroy(m_appOptions);
    SingletonRegistry::clear();
    AppContext::setApplication(nullptr);
}

AppOptions *ApplicationContext::options() const {
    return m_appOptions;
}
