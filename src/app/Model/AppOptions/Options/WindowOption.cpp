#include "WindowOption.h"

QByteArray WindowOption::decodeGeometry(const QJsonObject &object, const QString &key) {
    const auto value = object.value(key);
    if (!value.isString())
        return {};

    const auto encoded = value.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() % 4 != 0)
        return {};

    const auto decoded = QByteArray::fromBase64(encoded);
    if (decoded.isEmpty() || decoded.toBase64() != encoded)
        return {};

    return decoded;
}

void WindowOption::encodeGeometry(QJsonObject &object, const QString &key,
                                  const QByteArray &geometry) {
    if (!geometry.isEmpty())
        object.insert(key, QString::fromLatin1(geometry.toBase64()));
}

void WindowOption::load(const QJsonObject &object) {
    m_mainWindowGeometry = decodeGeometry(object, m_mainWindowGeometryKey);
    m_detachedPanelGeometry = decodeGeometry(object, m_detachedPanelGeometryKey);
}

void WindowOption::save(QJsonObject &object) {
    object = {};
    encodeGeometry(object, m_mainWindowGeometryKey, m_mainWindowGeometry);
    encodeGeometry(object, m_detachedPanelGeometryKey, m_detachedPanelGeometry);
}

const QByteArray &WindowOption::mainWindowGeometry() const {
    return m_mainWindowGeometry;
}

void WindowOption::setMainWindowGeometry(QByteArray geometry) {
    m_mainWindowGeometry = std::move(geometry);
}

const QByteArray &WindowOption::detachedPanelGeometry() const {
    return m_detachedPanelGeometry;
}

void WindowOption::setDetachedPanelGeometry(QByteArray geometry) {
    m_detachedPanelGeometry = std::move(geometry);
}
