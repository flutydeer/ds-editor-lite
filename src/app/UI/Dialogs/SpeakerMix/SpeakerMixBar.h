#ifndef DS_EDITOR_LITE_SPEAKERMIXBAR_H
#define DS_EDITOR_LITE_SPEAKERMIXBAR_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>

class SpeakerMixBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor segmentTextColor READ segmentTextColor WRITE setSegmentTextColor)
    Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor)
    Q_PROPERTY(QColor dividerDraggingColor READ dividerDraggingColor WRITE setDividerDraggingColor)

public:
    explicit SpeakerMixBar(QWidget *parent = nullptr);
    void setValues(const QVector<int> &values);
    void setDoubleValues(const QVector<double> &values);
    void setLabels(const QVector<QString> &labels);
    QVector<int> getValues() const;
    QVector<double> getDoubleValues() const;
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    static int getTotal() {
        return 100;
    }

    void setSegmentColors(const QVector<QColor> &colors);

    QVector<QColor> getSegmentColors() const {
        return m_segmentColors;
    }

signals:
    void valuesChanged(const QVector<double> &values);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateDividers();
    void setInternalValues(const QVector<double> &values);
    QVector<int> roundedValues() const;
    QRect getHandleRect(int dividerIndex) const;
    int findHandleAtPosition(const QPoint &position) const;
    int valueToPixel(double value) const;
    double pixelToSnappedValue(int pixel) const;
    [[nodiscard]] QColor trackColor() const;
    void setTrackColor(const QColor &color);
    [[nodiscard]] QColor segmentTextColor() const;
    void setSegmentTextColor(const QColor &color);
    [[nodiscard]] QColor dividerColor() const;
    void setDividerColor(const QColor &color);
    [[nodiscard]] QColor dividerDraggingColor() const;
    void setDividerDraggingColor(const QColor &color);

    QVector<double> m_values;
    QVector<QString> m_labels;
    QVector<double> m_dividers;
    QVector<double> m_dragStartValues;
    QVector<QColor> m_segmentColors;
    // Theme colors (QSS-overridable via qproperty-*); the segment text color
    // draws on top of speaker colors from AppColorPalette
    QColor m_trackColor = QColor("#2A2E38");
    QColor m_segmentTextColor = QColor("#111318");
    QColor m_dividerColor = QColor("#21242B");
    QColor m_dividerDraggingColor = QColor("#FFFFFF");
    int m_draggingIndex;
    int m_dragOffset;
    bool m_isDragging;
    bool m_readOnly;

    const int m_trackHeight = 40;
    const int m_handleWidth = 6;
    const int m_margin = 1;
    const int m_handleTouchMargin = 3;
};

#endif
