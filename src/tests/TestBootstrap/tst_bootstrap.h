#pragma once

#include <QObject>

class BootstrapTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidArguments_data();
    void invalidArguments();
    void validPort_data();
    void validPort();
    void flagsDoNotBecomeProjectPaths();
    void delimiterAndPreparseAgree();
    void identicalOptionsCanRepeat();
    void runtimeOverridesDoNotMutatePersistence();
    void initTestCase();
    void protocol();
    void identity();
    void automationBootstrap();
    void watcherLimit();
    void initialReadTimeout();
    void coordinator();
    void queuedStartupConnectionIsAcknowledged();
};
