#ifndef DS_EDITOR_LITE_SPEAKERMIXBAR_H
#define DS_EDITOR_LITE_SPEAKERMIXBAR_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>

class SpeakerMixBar : public QWidget {
    Q_OBJECT

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

    QVector<double> m_values;
    QVector<QString> m_labels;
    QVector<double> m_dividers;
    QVector<double> m_dragStartValues;
    QVector<QColor> m_segmentColors;
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
