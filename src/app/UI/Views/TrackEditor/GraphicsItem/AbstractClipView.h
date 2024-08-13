//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_CLIPGRAPHICSITEM_H
#define DATASET_TOOLS_CLIPGRAPHICSITEM_H

#include "Interface/IClip.h"
#include "Model/AppModel/Clip.h"
#include "UI/Utils/OverlappableItem.h"
#include "UI/Views/Common/CommonGraphicsRectItem.h"

class Menu;
class AbstractClipViewPrivate;

class AbstractClipView : public CommonGraphicsRectItem, public IClip, public OverlappableItem {
    Q_OBJECT

public:
    explicit AbstractClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AbstractClipView() override = default;

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
    void removeTriggered(int id);

protected:
    virtual QString text() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) = 0;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    virtual QString clipTypeName() const = 0;
    void setCanResizeLength(bool on);
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;

private:
    Q_DECLARE_PRIVATE(AbstractClipView)
    AbstractClipViewPrivate *d_ptr;
};



#endif // DATASET_TOOLS_CLIPGRAPHICSITEM_H
