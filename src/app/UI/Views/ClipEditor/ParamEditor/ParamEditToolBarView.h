#ifndef PARAMEDITTOOLBARVIEW_H
#define PARAMEDITTOOLBARVIEW_H

#include "ParamEditorEditMode.h"

#include <QWidget>

class Button;
class QButtonGroup;

class ParamEditToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditToolBarView(QWidget *parent = nullptr);
    void setParameter(ParamInfo::Name parameter);
    [[nodiscard]] ParamEditorEditMode editMode() const;
    [[nodiscard]] bool supportsEditMode(ParamEditorEditMode mode) const;
    bool setEditMode(ParamEditorEditMode mode);

signals:
    void editModeChanged(ParamEditorEditMode mode);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();

    Button *m_btnDraw = nullptr;
    Button *m_btnErase = nullptr;
    Button *m_btnBake = nullptr;
    Button *m_btnAnchor = nullptr;
    QButtonGroup *m_editModeGroup = nullptr;
};

#endif // PARAMEDITTOOLBARVIEW_H
