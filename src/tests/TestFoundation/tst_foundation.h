#pragma once

#include <QObject>

int runLogFixture(const QString &directory);

class FoundationTests final : public QObject {
    Q_OBJECT

private slots:
    void expectedSuccessKeepsValue();
    void expectedFailureKeepsError();
    void expectedFallbackIsLazy();
    void expectedMappingPropagatesErrors();
    void localizedTextLookup_data();
    void localizedTextLookup();
    void localizedTextSingleTagOverload();
    void fileLoggingChangesDirectoriesAndRecoversFromWriteFailure();
};
