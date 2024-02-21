#ifndef DS_EDITOR_LITE_TABLECONFIGWIDGET_H
#define DS_EDITOR_LITE_TABLECONFIGWIDGET_H

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <QVBoxLayout>
#include <QHBoxLayout>

namespace FillLyric {

    class TableConfigWidget : public QWidget {
        Q_OBJECT
        friend class LyricWidget;

    public:
        explicit TableConfigWidget(QWidget *parent = nullptr);
        ~TableConfigWidget() override;

    protected:
        QDoubleSpinBox *m_aspectRatioSpinBox;
        QSpinBox *m_fontDiffSpinBox;

    private:
        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_fontLayout;
    };
} // FillLyric

#endif // DS_EDITOR_LITE_TABLECONFIGWIDGET_H
