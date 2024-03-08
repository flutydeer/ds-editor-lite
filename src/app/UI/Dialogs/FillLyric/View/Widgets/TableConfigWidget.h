#ifndef DS_EDITOR_LITE_TABLECONFIGWIDGET_H
#define DS_EDITOR_LITE_TABLECONFIGWIDGET_H

#include <QLabel>
#include <QWidget>
#include <QDoubleSpinBox>

#include <QHBoxLayout>

namespace FillLyric {

    class TableConfigWidget final: public QWidget {
        Q_OBJECT
        friend class LyricTab;

    public:
        explicit TableConfigWidget(QWidget *parent = nullptr);
        ~TableConfigWidget() override;

    protected:
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
