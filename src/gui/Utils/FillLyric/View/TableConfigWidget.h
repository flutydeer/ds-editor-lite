#ifndef DS_EDITOR_LITE_TABLECONFIGWIDGET_H
#define DS_EDITOR_LITE_TABLECONFIGWIDGET_H

#include <QLabel>
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
        QLabel *m_warningLabel;

        QLabel *m_colWidthRatioLabel;
        QLabel *m_rowHeightLabel;
        QLabel *m_fontDiffLabel;

        QDoubleSpinBox *m_colWidthRatioSpinBox;
        QDoubleSpinBox *m_rowHeightSpinBox;
        QSpinBox *m_fontDiffSpinBox;

    private:
        QVBoxLayout *m_mainLayout;
    };
} // FillLyric

#endif // DS_EDITOR_LITE_TABLECONFIGWIDGET_H
