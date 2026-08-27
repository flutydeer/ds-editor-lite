#ifndef PUBLICCOLLECTIONPAGINATION_H
#define PUBLICCOLLECTIONPAGINATION_H

#include "../AutomationTypes.h"

#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace Automation::PublicRegistryDetail {

    struct JsonPage {
        QJsonArray items;
        QString nextCursor;
    };

    inline AutomationResult<JsonPage> paginateJson(AutomationWire::OpaqueCursorCodec &codec,
                                                   const QJsonArray &all,
                                                   const QJsonObject &arguments,
                                                   const QString &scope,
                                                   const QJsonValue &identity) {
        const auto digest = AutomationWire::sha256Digest(identity);
        qint64 offset = 0;
        const auto cursor = arguments.value(QStringLiteral("cursor")).toString();
        if (!cursor.isEmpty()) {
            const auto parsed = codec.parse(cursor, scope, digest);
            if (!parsed.valid()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("cursor"), QStringLiteral("Collection cursor is invalid"));
            }
            offset = *parsed.offset;
        }
        if (offset < 0 || offset > all.size()) {
            return AutomationError::invalidArgument(QStringLiteral("cursor"),
                                                    QStringLiteral("Collection cursor is invalid"));
        }
        const auto requested = arguments.value(QStringLiteral("limit")).toInt(all.size());
        const auto end = std::min<qint64>(all.size(), offset + requested);
        JsonPage result;
        for (auto index = offset; index < end; ++index)
            result.items.append(all.at(static_cast<qsizetype>(index)));
        if (end < all.size())
            result.nextCursor = codec.issue(scope, digest, end);
        return result;
    }

} // namespace Automation::PublicRegistryDetail

#endif // PUBLICCOLLECTIONPAGINATION_H
