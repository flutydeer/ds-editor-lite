//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_CLIPGRAPHICSITEM_H
#define DATASET_TOOLS_CLIPGRAPHICSITEM_H

#include "Interface/IClip.h"
#include "Model/AppModel/Clip.h"
#include "UI/Utils/OverlappableItem.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"

class Menu;
class AbstractClipViewPrivate;

class AbstractClipView : public AbstractGraphicsRectItem, public IClip {
    Q_OBJECT

public:
    explicit AbstractClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AbstractClipView() override;

    QString name() const override;
    void setName(const QString &text) override;

    // QColor color() const;
    // void setColor(const QColor &color);

    int start() const override;
    void setStart(int start) override;
    int length() const override;
    void setLength(int length) override;
    int clipStart() const override;
    void setClipStart(int clipStart) override;
    int clipLen() const override;
    void setClipLen(int clipLen) override;

    double gain() const override;
    void setGain(double gain) override;
    // double pan() const;
    // void setPan(double gain);
    bool mute() const override;
    void setMute(bool mute) override;

    bool activeClip() const;
    void setActiveClip(bool active);

    int trackIndex() const;
    void setTrackIndex(int index);
    bool canResizeLength() const;
    virtual int contentLength() const;

    void loadCommonProperties(const Clip::ClipCommonProperties &args);

public slots:
    void setQuantize(int quantize);

signals:
    void removeTriggered(int id);

protected:
    virtual QString text() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) = 0;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void updateRectAndPos() override;
    virtual QString clipTypeName() const = 0;
    virtual QString iconPath() const = 0;
    void setCanResizeLength(bool on);
    double tickToSceneX(double tick) const;
    double sceneXToItemX(double x) const;

private:
    Q_DECLARE_PRIVATE(AbstractClipView)
    AbstractClipViewPrivate *d_ptr;
};



#endif // DATASET_TOOLS_CLIPGRAPHICSITEM_H
