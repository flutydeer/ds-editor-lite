#include "ProjectSnapshot.h"

#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <opendspx/serializer/serializer.h>

#include <QJsonDocument>
#include <QtTest/QTest>

#include <sstream>

namespace TestSupport {
    QJsonObject projectSnapshot(const AppModel &model) {
        const auto snapshot = DspxProjectConverter{}.encodeProject({}, &model);
        opendspx::SerializationErrorList errors;
        std::ostringstream output;
        // Capture invalid drafts too; save workflows exercise validation separately.
        opendspx::Serializer::serialize(output, snapshot, errors, {});
        QJsonParseError error;
        const auto document =
            QJsonDocument::fromJson(QByteArray::fromStdString(output.str()), &error);
        if (!errors.empty() || error.error != QJsonParseError::NoError || !document.isObject()) {
            const auto message =
                QStringLiteral("Cannot capture project content: %1").arg(error.errorString());
            QTest::qFail(qPrintable(message), __FILE__, __LINE__);
            return {};
        }
        return document.object();
    }
}
