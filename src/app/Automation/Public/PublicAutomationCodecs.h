#ifndef PUBLICAUTOMATIONCODECS_H
#define PUBLICAUTOMATIONCODECS_H

#include "Automation/AutomationTypes.h"
#include "Automation/DocumentSession.h"
#include "Utils/UiLanguageManager.h"

#include <lite/AutomationWire/PublicEnums.h>
#include <lite/Support/LocalizedTextUtils.h>

#include <QJsonObject>
#include <QMap>

namespace Automation {

    inline QString encodePublicDocumentLifecycle(const DocumentLifecycleState value) {
        using WireLifecycle = AutomationWire::DocumentLifecycle;
        switch (value) {
            case DocumentLifecycleState::Active:
                return AutomationWire::documentLifecycleName(WireLifecycle::Active);
            case DocumentLifecycleState::Replacing:
                return AutomationWire::documentLifecycleName(WireLifecycle::Replacing);
            case DocumentLifecycleState::Closing:
                return AutomationWire::documentLifecycleName(WireLifecycle::Closing);
        }
        return {};
    }

    inline QString encodePublicObjectKind(const ObjectKind value) {
        using WireKind = AutomationWire::PublicObjectKind;
        switch (value) {
            case ObjectKind::Unknown:
                return AutomationWire::publicObjectKindName(WireKind::Unknown);
            case ObjectKind::Track:
                return AutomationWire::publicObjectKindName(WireKind::Track);
            case ObjectKind::Clip:
                return AutomationWire::publicObjectKindName(WireKind::Clip);
            case ObjectKind::Note:
                return AutomationWire::publicObjectKindName(WireKind::Note);
            case ObjectKind::InferPiece:
                return AutomationWire::publicObjectKindName(WireKind::InferPiece);
            case ObjectKind::Curve:
                return AutomationWire::publicObjectKindName(WireKind::Curve);
            case ObjectKind::Anchor:
                return AutomationWire::publicObjectKindName(WireKind::Anchor);
            case ObjectKind::SpeakerMixKeyframe:
                return AutomationWire::publicObjectKindName(WireKind::SpeakerMixKeyframe);
        }
        return {};
    }

    inline QJsonObject encodePublicAutomationError(const AutomationError &error) {
        QJsonObject result{
            {QStringLiteral("code"),    errorCodeName(error.code)},
            {QStringLiteral("message"), error.message            },
        };
        if (!error.operationId.isEmpty())
            result.insert(QStringLiteral("operation_id"), error.operationId);
        if (!error.fieldPath.isEmpty())
            result.insert(QStringLiteral("field_path"), error.fieldPath);
        if (error.object) {
            result.insert(QStringLiteral("object"),
                          QJsonObject{
                              {QStringLiteral("kind"), encodePublicObjectKind(error.object->kind)},
                              {QStringLiteral("id"),   error.object->value                       },
            });
        }
        if (error.taskId)
            result.insert(QStringLiteral("task_id"), error.taskId->toString());
        if (error.documentId)
            result.insert(QStringLiteral("document_id"), error.documentId->toString());
        if (error.expectedRevision) {
            result.insert(QStringLiteral("expected_revision"),
                          static_cast<qint64>(*error.expectedRevision));
        }
        if (error.actualRevision) {
            result.insert(QStringLiteral("actual_revision"),
                          static_cast<qint64>(*error.actualRevision));
        }
        return result;
    }

    inline QString resolvePublicDisplayText(const QString &defaultText,
                                            const QMap<QString, QString> &localized) {
        return lite::Support::lookupLocalizedText(localized, defaultText,
                                                  UiLanguageManager::currentBcp47Candidates());
    }

    inline QJsonObject encodePublicLocalizedText(const QMap<QString, QString> &localized) {
        QJsonObject result;
        for (auto it = localized.cbegin(); it != localized.cend(); ++it)
            result.insert(it.key(), it.value());
        return result;
    }

    inline bool publicLocalizedTextContains(const QString &query, const QString &defaultText,
                                            const QMap<QString, QString> &localized) {
        if (query.isEmpty() || defaultText.contains(query, Qt::CaseInsensitive))
            return true;
        for (auto it = localized.cbegin(); it != localized.cend(); ++it) {
            if (it.value().contains(query, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

} // namespace Automation

#endif // PUBLICAUTOMATIONCODECS_H
