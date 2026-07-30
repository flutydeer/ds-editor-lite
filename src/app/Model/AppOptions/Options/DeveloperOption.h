//
// Created by fluty on 26-5-8.
//

#ifndef DEVELOPEROPTION_H
#define DEVELOPEROPTION_H

#include "Model/AppOptions/IOption.h"
#include <lite/ADT/Property.h>

class DeveloperOption final : public IOption {
public:
    enum class EditorRenderBackend { Legacy, RhiExperimental };

    explicit DeveloperOption() : IOption("developer") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(bool, enableDiagnostics, false)
    LITE_OPTION_ITEM(bool, showLogWindow, false)
    LITE_OPTION_ITEM(bool, showTimelineDebugInfo, false)
    LITE_OPTION_ITEM(bool, showClipDebugInfo, false)
    LITE_OPTION_ITEM(bool, enablePanelDetach, false)

public:
    EditorRenderBackend editorRenderBackend = EditorRenderBackend::Legacy;

    [[nodiscard]] static QString editorRenderBackendToString(EditorRenderBackend backend);
    [[nodiscard]] static EditorRenderBackend editorRenderBackendFromString(const QString &value);
};


#endif // DEVELOPEROPTION_H
