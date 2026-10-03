#pragma once

#include <QObject>
#include <QString>

class ProcessIntegrationTests final : public QObject {
    Q_OBJECT

public:
    QString editorPath;
    QString connectorPath;
    QString platformPluginDirectory;

private slots:
    void nativeEditingAndFiles();
    void audioImportAndWaveExport();
    void consoleTermination_data();
    void consoleTermination();
    void occupiedPort();
    void restart();
    void isolatedPrimaries();
    void crossHost();
    void editingWorkflow();
    void secondaryStartup();
    void legacyConnector();
    void legacyEditor();
    void documentLifecycle();
    void gracefulExit();
};
