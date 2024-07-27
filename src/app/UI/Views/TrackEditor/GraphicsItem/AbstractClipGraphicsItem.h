//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_CLIPGRAPHICSITEM_H
#define DATASET_TOOLS_CLIPGRAPHICSITEM_H

#include "Interface/IClip.h"
#include "Model/Clip.h"
#include "UI/Views/Common/CommonGraphicsRectItem.h"
#include "Utils/IOverlapable.h"

class Menu;
class AbstractClipGraphicsItemPrivate;

class AbstractClipGraphicsItem : public CommonGraphicsRectItem, public IClip, public IOverlapable {
    Q_OBJECT

public:
    explicit AbstractClipGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    ~AbstractClipGraphicsItem() override = default;

    [[nodiscard]] QString name() const override;
    void setName(const QString &text) override;

    // QColor color() const;
    // void setColor(const QColor &color);

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
    // double pan() const;
    // void setPan(double gain);
    [[nodiscard]] bool mute() const override;
    void setMute(bool mute) override;

    [[nodiscard]] int trackIndex() const;
    void setTrackIndex(int index);
    [[nodiscard]] bool canResizeLength()const;

    void loadCommonProperties(const Clip::ClipCommonProperties &args);

public slots:
    void setQuantize(int quantize);

signals:
    void propertyChanged();
    void removeTriggered(int id);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) = 0;
    // void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    virtual QString clipTypeName() = 0;
    void setCanResizeLength(bool on);
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;

private:
    Q_DECLARE_PRIVATE(AbstractClipGraphicsItem)
    AbstractClipGraphicsItemPrivate *d_ptr;
};



#endif // DATASET_TOOLS_CLIPGRAPHICSITEM_H
