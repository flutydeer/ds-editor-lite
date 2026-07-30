//
// Created by fluty on 26-5-8.
//

#ifndef DEVELOPEROPTION_H
#define DEVELOPEROPTION_H

#include "Model/AppOptions/IOption.h"
#include "UI/Views/EditorCanvas/EditorCanvasTypes.h"
#include <lite/ADT/Property.h>

class DeveloperOption final : public IOption {
public:
    explicit DeveloperOption() : IOption("developer") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    [[nodiscard]] EditorCanvasBackend editorCanvasBackend() const;
    void setEditorCanvasBackend(EditorCanvasBackend backend);

    LITE_OPTION_ITEM(bool, enableDiagnostics, false)
    LITE_OPTION_ITEM(bool, showLogWindow, false)
    LITE_OPTION_ITEM(bool, showTimelineDebugInfo, false)
    LITE_OPTION_ITEM(bool, showClipDebugInfo, false)
    LITE_OPTION_ITEM(bool, enablePanelDetach, false)
    LITE_OPTION_ITEM(QString, editorRenderer, QStringLiteral("legacy"))
};


#endif // DEVELOPEROPTION_H
