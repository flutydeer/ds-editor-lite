#ifndef DSSPPAGE_H
#define DSSPPAGE_H

#include "IOptionPage.h"

class SwitchButton;
class LineEdit;
class QSpinBox;

class DsspPage : public IOptionPage {
    Q_OBJECT

public:
    explicit DsspPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    SwitchButton *m_swEnabled;
    LineEdit *m_leHost;
    QSpinBox *m_spPort;
};

#endif // DSSPPAGE_H
