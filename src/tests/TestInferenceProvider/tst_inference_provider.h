#pragma once

#include <QObject>

class InferenceProviderTests final : public QObject {
    Q_OBJECT
private slots:
    void supportedProviders();
    void cudaProvider();
};
