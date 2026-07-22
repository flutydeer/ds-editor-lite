//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef BOTTOMPANELVIEW_H
#define BOTTOMPANELVIEW_H

#include "UI/Views/Common/TabPanelView.h"

class ClipEditorView;

class BottomPanelView : public TabPanelView {
    Q_OBJECT

public:
    explicit BottomPanelView(QWidget *parent = nullptr);
    ~BottomPanelView() override;
    [[nodiscard]] ClipEditorView *clipEditorView() const;

private:
    ClipEditorView *m_clipEditorView;
};

#endif // BOTTOMPANELVIEW_H
