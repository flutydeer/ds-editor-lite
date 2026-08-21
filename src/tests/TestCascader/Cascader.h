#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QPainter;
class QRectF;

// A single tree node handed to Cascader. Mirrors Element Plus's data shape
// (value + label + children) so callers can reuse their existing tree structs
// with a small adaptor. value may be any QVariant-capable type; label is what
// gets displayed.
struct CascaderNode {
    QVariant value;
    QString label;
    QList<CascaderNode> children;

    [[nodiscard]] bool isLeaf() const {
        return children.isEmpty();
    }
};

// Element Plus style cascader (simplified):
//  - readonly input-like trigger showing the selected label path
//  - click opens a multi-column popup; columns advance as you pick parent nodes
//  - clicking a leaf node commits the selection and closes the popup
//  - the popup is a plain Qt::Popup widget, never a QMenu
class Cascader : public QWidget {
    Q_OBJECT
public:
    explicit Cascader(QWidget *parent = nullptr);
    ~Cascader() override;

    void setOptions(const QList<CascaderNode> &rootNodes);

    [[nodiscard]] const QList<CascaderNode> &options() const {
        return m_options;
    }

    // The value path of the current selection, e.g. { 1, "s1" }.
    [[nodiscard]] QList<QVariant> currentValue() const {
        return m_valuePath;
    }

    void setCurrentValue(const QList<QVariant> &path);

    // The label path, joined with " / ", e.g. "Zone 1 / Group A".
    [[nodiscard]] QString currentText() const;

    void setPlaceholderText(const QString &text);

    [[nodiscard]] QString placeholderText() const {
        return m_placeholder;
    }

signals:
    void currentValueChanged(const QList<QVariant> &value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    friend class CascaderPopup;
    void rebuildPopups();
    bool resolvePath(const QList<QVariant> &value, QList<CascaderNode> *breadcrumb) const;
    void drawArrow(QPainter *painter, const QRectF &rect, const QColor &color);

    QList<CascaderNode> m_options;
    QList<QVariant> m_valuePath;
    QString m_placeholder;
    CascaderPopup *m_popup = nullptr;
    bool m_ignoreNextShow = false;
};
