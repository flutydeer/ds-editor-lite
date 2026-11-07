#ifndef LITE_ADAPTIVETOOLBAR_H
#define LITE_ADAPTIVETOOLBAR_H

#include <QWidget>

#include <memory>

class Button;
class AdaptiveToolBarPrivate;

class AdaptiveToolBar final : public QWidget {
    Q_OBJECT

public:
    struct ItemOptions {
        int group = 0;
        int order = 0;
        // Lower priorities overflow first. Presentation remains sorted by group and order.
        int overflowPriority = 0;
    };

    explicit AdaptiveToolBar(QWidget *parent = nullptr);
    ~AdaptiveToolBar() override;

    void addWidget(QWidget *widget, const ItemOptions &options);
    // Semantic applicability is independent of the widget's visibility and enabled state.
    void setItemApplicable(QWidget *widget, bool applicable);
    bool isItemApplicable(QWidget *widget) const;
    bool isWidgetOverflowed(QWidget *widget) const;
    void invalidateItemSize(QWidget *widget = nullptr);
    void setItemSpacing(int spacing);
    void setGroupSpacing(int spacing);

    // Each registered group owns a trailing More button and its own original-widget popup.
    Button *overflowButton(int group = 0) const;
    QWidget *overflowPopup(int group = 0) const;
    void closeOverflow();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void widgetAboutToRelocate(QWidget *widget, QWidget *destinationHost);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    friend class AdaptiveToolBarPrivate;
    std::unique_ptr<AdaptiveToolBarPrivate> d;
};

#endif
