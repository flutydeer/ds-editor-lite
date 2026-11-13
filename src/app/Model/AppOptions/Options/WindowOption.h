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

    // The detached editor panel remembers its own placement. It is created
    // lazily, so an empty value simply means "never detached on this machine".
    [[nodiscard]] const QByteArray &detachedPanelGeometry() const;
    void setDetachedPanelGeometry(QByteArray geometry);

private:
    // Both geometries use the same wire format: base64 of QWidget::saveGeometry(),
    // stored as a JSON string. A value that does not round-trip exactly is
    // rejected instead of handed to restoreGeometry().
    static QByteArray decodeGeometry(const QJsonObject &object, const QString &key);
    static void encodeGeometry(QJsonObject &object, const QString &key,
                               const QByteArray &geometry);

    const QString m_mainWindowGeometryKey = "mainWindowGeometry";
    const QString m_detachedPanelGeometryKey = "detachedPanelGeometry";
    QByteArray m_mainWindowGeometry;
    QByteArray m_detachedPanelGeometry;
};

#endif // WINDOWOPTION_H
