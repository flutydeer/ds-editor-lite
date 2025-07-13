//
// Created by FlutyDeer on 2025/7/13.
//

#include "BottomPanelView.h"

#include "Controller/AppController.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"

BottomPanelView::BottomPanelView(QWidget *parent) : TabPanelView(AppGlobal::ClipEditor, parent) {
    // TODO: 重构，不应使用 ClipEditor 类型
    registerPage(new ClipEditorView);
    registerPage(new MixConsoleView);

    appController->registerPanel(this);
}