#ifndef SEEKBAR_H
#define SEEKBAR_H

#include <functional>

#include <QWidget>

namespace SVS {

    class SeekBarPrivate;

    class SeekBar : public QWidget {
        Q_OBJECT
        Q_DECLARE_PRIVATE(SeekBar)
        Q_PROPERTY(QColor trackInactiveColor READ trackInactiveColor WRITE setTrackInactiveColor)
        Q_PROPERTY(QColor trackActiveColor READ trackActiveColor WRITE setTrackActiveColor)
        Q_PROPERTY(QColor thumbFillColor READ thumbFillColor WRITE setThumbFillColor)
        Q_PROPERTY(QColor thumbBorderColor READ thumbBorderColor WRITE setThumbBorderColor)
        Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)
        Q_PROPERTY(bool resetOnDoubleClick READ resetOnDoubleClick WRITE setResetOnDoubleClick)
    public:
        explicit SeekBar(QWidget *parent = nullptr);
        ~SeekBar() override;

        double maximum() const;
        void setMaximum(double maximum);
        double minimum() const;
        void setMinimum(double minimum);
        void setRange(double minimum, double maximum);

        double interval() const;
        void setInterval(double interval);

        double singleStep() const;
        void setSingleStep(double step);
        double pageStep() const;
        void setPageStep(double step);

        bool hasTracking() const;
        void setTracking(bool enable);

        bool isSliderDown() const;
        void setSliderDown(bool down);

        double sliderPosition() const;
        void setSliderPosition(double position);

        double trackActiveStartValue() const;
        void setTrackActiveStartValue(double pos);

        double value() const;

        double defaultValue() const;
        void setDefaultValue(double value);

        double displayValue() const;
        void setDisplayValueConverter(const std::function<double(double)> &converter);

        int animationDuration() const;
        void setAnimationDuration(int dur);

        bool resetOnDoubleClick() const;
        void setResetOnDoubleClick(bool);

    public Q_SLOTS:
        void setValue(double value);
        void resetValue();

    Q_SIGNALS:
        void sliderPressed();
        void sliderReleased();
        void sliderMoved(double value);
        void valueChanged(double value);

    protected:
        bool eventFilter(QObject *object, QEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

        explicit SeekBar(QWidget *parent, SeekBarPrivate &d);

    private:
        QScopedPointer<SeekBarPrivate> d_ptr;

        QColor trackInactiveColor() const;
        void setTrackInactiveColor(const QColor &color);
        QColor trackActiveColor() const;
        void setTrackActiveColor(const QColor &color);
        QColor thumbFillColor() const;
        void setThumbFillColor(const QColor &color);
        QColor thumbBorderColor() const;
        void setThumbBorderColor(const QColor &color);
    };

} // SVS

#endif // SEEKBAR_H
