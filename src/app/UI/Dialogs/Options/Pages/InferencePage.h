#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"

class LineEdit;
class ComboBox;

class InferencePage : public IOptionPage {
    Q_OBJECT
public:
    explicit InferencePage(QWidget *parent = nullptr);
    ~InferencePage() override;

protected:
    void modifyOption() override;

private:
    ComboBox *m_cbExecutionProvider;
    ComboBox *m_cbDeviceList;
    ComboBox *m_cbDsSpeedup;
    LineEdit *m_leDsDepth;
};



#endif //INFERENCEPAGE_H
