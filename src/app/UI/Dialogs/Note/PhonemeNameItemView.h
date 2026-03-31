//
// Created by FlutyDeer on 2026/4/1.
//

#ifndef DS_EDITOR_LITE_PHONEMENAMEITEMVIEW_H
#define DS_EDITOR_LITE_PHONEMENAMEITEMVIEW_H

#include <QCheckBox>
#include <QWidget>

class LanguageComboBox;
class LineEdit;

class PhonemeNameItemView : public QWidget {
    Q_OBJECT

public:
    explicit PhonemeNameItemView(QWidget *parent = nullptr);

    LanguageComboBox *cbLanguage() const;
    LineEdit *leName() const;
    QCheckBox *cbIsOnset() const;

private:
    LanguageComboBox *m_cbLanguage = nullptr;
    LineEdit *m_leName = nullptr;
    QCheckBox *m_cbIsOnset = nullptr;
};



#endif //DS_EDITOR_LITE_PHONEMENAMEITEMVIEW_H
