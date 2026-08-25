#include "WindowOption.h"

void WindowOption::load(const QJsonObject &object) {
    m_mainWindowGeometry.clear();

    const auto value = object.value(m_mainWindowGeometryKey);
    if (!value.isString())
        return;

    const auto encoded = value.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() % 4 != 0)
        return;

    const auto decoded = QByteArray::fromBase64(encoded);
    if (decoded.isEmpty() || decoded.toBase64() != encoded)
        return;

    m_mainWindowGeometry = decoded;
}

void WindowOption::save(QJsonObject &object) {
    object = {};
    if (!m_mainWindowGeometry.isEmpty())
        object.insert(m_mainWindowGeometryKey,
                      QString::fromLatin1(m_mainWindowGeometry.toBase64()));
}

const QByteArray &WindowOption::mainWindowGeometry() const {
    return m_mainWindowGeometry;
}

void WindowOption::setMainWindowGeometry(QByteArray geometry) {
    m_mainWindowGeometry = std::move(geometry);
}
