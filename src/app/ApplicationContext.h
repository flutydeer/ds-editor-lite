#ifndef APPLICATIONCONTEXT_H
#define APPLICATIONCONTEXT_H

#include <memory>

#include <QtGlobal>

class AppOptions;
class AudioSystemContext;
class InferEngine;
class PackageManager;
class ProjectFormatRegistry;
class SynthrtEngine;

class ApplicationContext final {
public:
    explicit ApplicationContext(std::unique_ptr<AppOptions> options);
    ~ApplicationContext();

    Q_DISABLE_COPY_MOVE(ApplicationContext)

    AppOptions *options() const;

private:
    AppOptions *m_appOptions = nullptr;
    PackageManager *m_packageManager = nullptr;
    ProjectFormatRegistry *m_projectFormatRegistry = nullptr;
    SynthrtEngine *m_synthrtEngine = nullptr;
    InferEngine *m_inferEngine = nullptr;
    std::unique_ptr<AudioSystemContext> m_audioSystem;

#if defined(WITH_DIRECT_MANIPULATION)
    std::unique_ptr<struct DirectManipulationHolder> m_directManip;
#endif
};

#endif // APPLICATIONCONTEXT_H
