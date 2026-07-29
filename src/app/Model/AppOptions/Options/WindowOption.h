#ifndef WINDOWOPTION_H
#define WINDOWOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QByteArray>

class WindowOption final : public IOption {
public:
    explicit WindowOption() : IOption("window") {
    }

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    [[nodiscard]] const QByteArray &mainWindowGeometry() const;
    void setMainWindowGeometry(QByteArray geometry);

private:
    const QString m_mainWindowGeometryKey = "mainWindowGeometry";
    QByteArray m_mainWindowGeometry;
};

#endif // WINDOWOPTION_H
