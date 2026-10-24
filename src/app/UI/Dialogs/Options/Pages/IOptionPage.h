#ifndef OPTIONPAGE_H
#define OPTIONPAGE_H

#include <QMargins>
#include <QScrollArea>

class IOptionPage : public QScrollArea {
    Q_OBJECT

public:
    explicit IOptionPage(QWidget *parent = nullptr);

protected:
    virtual void modifyOption() = 0;
    virtual QWidget *createContentWidget() = 0;
    // Insets applied to the page's content widget. Pages whose content manages
    // its own side insets (e.g. an inner scroll area that has to span to the
    // window edge so its overlay scrollbar pins there) override this.
    [[nodiscard]] virtual QMargins contentMargins() const;
    void changeEvent(QEvent *event) override;
    void initializePage();
    // Re-runs the page sizing (and the scroll range) after the page's visible
    // content changed, e.g. a sub-page switch that shows a taller/shorter
    // widget.
    void refitContent();

private:
    // Keeps the content widget's explicit minimum in sync with the layout's
    // heightForWidth state (see syncContentMinimum in the .cpp).
    void syncContentMinimum();
    bool m_retranslatePending = false;
};

#endif // OPTIONPAGE_H
