#ifndef PARAMEDITTOOLBARVIEW_H
#define PARAMEDITTOOLBARVIEW_H

#include "ParamEditorEditMode.h"

#include <QColor>
#include <QList>
#include <QPair>
#include <QWidget>

class Button;
class QButtonGroup;

class ParamEditToolBarView final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)
    Q_PROPERTY(QColor iconOnColor READ iconOnColor WRITE setIconOnColor)
    Q_PROPERTY(QColor iconOnDisabledColor READ iconOnDisabledColor WRITE setIconOnDisabledColor)

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

    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated button icons
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);
    [[nodiscard]] QColor iconDisabledColor() const;
    void setIconDisabledColor(const QColor &color);
    [[nodiscard]] QColor iconOnColor() const;
    void setIconOnColor(const QColor &color);
    [[nodiscard]] QColor iconOnDisabledColor() const;
    void setIconOnDisabledColor(const QColor &color);
    void rebuildIcons();

    Button *m_btnDraw = nullptr;
    Button *m_btnShape = nullptr;
    Button *m_btnScale = nullptr;
    Button *m_btnErase = nullptr;
    Button *m_btnBake = nullptr;
    Button *m_btnAnchor = nullptr;
    QButtonGroup *m_editModeGroup = nullptr;

    // Theme colors (owned here, QSS-overridable via qproperty-*)
    QColor m_iconColor = {240, 240, 240};
    QColor m_iconDisabledColor = {240, 240, 240, 102};
    QColor m_iconOnColor = {155, 186, 255};
    QColor m_iconOnDisabledColor = {155, 186, 255, 102};
    // Buttons with tinted SVG icons and their source paths, for re-tinting
    QList<QPair<Button *, QString>> m_tintedButtons;
};

#endif // PARAMEDITTOOLBARVIEW_H
