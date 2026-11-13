#ifndef DATASET_TOOLS_CLIPGRAPHICSITEM_H
#define DATASET_TOOLS_CLIPGRAPHICSITEM_H

#include <lite/ProjectModel/Interface/IClip.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/GUI/Base/OverlappableItem.h>
#include "UI/Views/Common/AbstractGraphicsRectItem.h"

class Menu;
class AbstractClipViewPrivate;

class AbstractClipView : public AbstractGraphicsRectItem, public IClip {
    Q_OBJECT

public:
    explicit AbstractClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AbstractClipView() override;

    static QColor selectedBorderColor();
    static void setSelectedBorderColor(const QColor &color);

    [[nodiscard]] QString name() const override;
    void setName(const QString &text) override;

    [[nodiscard]] int colorIndex() const;
    void setColorIndex(int index);

    [[nodiscard]] int start() const override;
    void setStart(int start) override;
    [[nodiscard]] int length() const override;
    void setLength(int length) override;
    [[nodiscard]] int clipStart() const override;
    void setClipStart(int clipStart) override;
    [[nodiscard]] int clipLen() const override;
    void setClipLen(int clipLen) override;

    [[nodiscard]] double gain() const override;
    void setGain(double gain) override;
    [[nodiscard]] bool mute() const override;
    void setMute(bool mute) override;

    [[nodiscard]] bool activeClip() const;
    void setActiveClip(bool active);

    [[nodiscard]] int trackIndex() const;
    void setTrackIndex(int index);
    [[nodiscard]] bool canResizeLength() const;
    [[nodiscard]] virtual int contentLength() const;

    void loadCommonProperties(const Clip::ClipCommonProperties &args);

    // The band that carries the name, in item coordinates. Empty when the clip
    // is too short to draw the title as its own strip, which is exactly when it
    // does not own any double click.
    [[nodiscard]] QRectF titleBandRect() const;
    // Whether a point in item coordinates may start the inline title rename.
    [[nodiscard]] bool acceptsTitleRename(const QPointF &itemPos) const;
    // While a rename is open the item stops painting the title: the overlay is
    // the only place the old name shows.
    void setTitleEditing(bool editing);
    [[nodiscard]] bool isTitleEditing() const;

signals:
    void removeTriggered(int id);

protected:
    [[nodiscard]] virtual QString text() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) = 0;
    [[nodiscard]] QRectF previewRect() const;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    [[nodiscard]] virtual QString clipTypeName() const = 0;
    [[nodiscard]] virtual QString iconPath() const = 0;
    void setCanResizeLength(bool on);
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;
    [[nodiscard]] double leftMarginPx() const;

private:
    Q_DECLARE_PRIVATE(AbstractClipView)
    AbstractClipViewPrivate *d_ptr;

    static QColor s_selectedBorderColor;
};



#endif // DATASET_TOOLS_CLIPGRAPHICSITEM_H
