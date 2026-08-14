#ifndef DSSP_CONTROLLER_H
#define DSSP_CONTROLLER_H

#include <lite/Core/Singleton.h>

#include <QObject>

class DsspServer;

/// App-level controller owning the DSSP HTTP service lifecycle.
///
/// Constructed after SynthrtEngine/InferEngine and destroyed before them
/// (AppContext reverse-order teardown), so the server always stops — waiting
/// for in-flight synthesis on its worker threads — before the inference
/// environment is destroyed.
class DsspController final : public QObject {
    Q_OBJECT

private:
    friend class SingletonRegistry; // constructed/destroyed via SingletonRegistry::create/destroy
    explicit DsspController(QObject *parent = nullptr);
    ~DsspController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(DsspController)
    Q_DISABLE_COPY_MOVE(DsspController)

    bool isServiceRunning() const;

private:
    void applySettings();
    void startService();
    void stopService();

    DsspServer *m_server = nullptr;
    QString m_lastHost;
    int m_lastPort = 0;
};

#endif // DSSP_CONTROLLER_H
