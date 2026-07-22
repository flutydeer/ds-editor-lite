//
// Created by FlutyDeer on 2025/7/13.
//

#include "BottomPanelView.h"

#include "Controller/EditorViewController.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"

BottomPanelView::BottomPanelView(QWidget *parent) : TabPanelView(AppGlobal::ClipEditor, parent) {
    m_clipEditorView = new ClipEditorView;
    registerPage(m_clipEditorView);
    registerPage(new MixConsoleView);

    setPanelType(currentPagePanelType());
    connect(this, &TabPanelView::currentPageChanged, this,
            [this](const QString &, const AppGlobal::PanelType panelType) {
                setPanelType(panelType);
                editorViewController->setActivePanel(panelType);
            });
    editorViewController->registerPanel(this);
}

BottomPanelView::~BottomPanelView() {
    editorViewController->unregisterPanel(this);
}

ClipEditorView *BottomPanelView::clipEditorView() const {
    return m_clipEditorView;
}
