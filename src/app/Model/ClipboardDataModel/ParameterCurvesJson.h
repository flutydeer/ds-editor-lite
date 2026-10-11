#ifndef PARAMETERCURVESJSON_H
#define PARAMETERCURVESJSON_H

#include "Automation/ProjectAutomationDtos.h"

#include <QJsonArray>

namespace ClipboardDataModel {
    QJsonArray serializeParameters(const QList<Automation::ParamCurvesDraftDto> &parameters);
    QList<Automation::ParamCurvesDraftDto> deserializeParameters(const QJsonArray &parameters);
} // namespace ClipboardDataModel

#endif // PARAMETERCURVESJSON_H
