#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"

class InferencePageWidget;

class InferencePage : public IOptionPage {
    Q_OBJECT
public:
    explicit InferencePage(QWidget *parent = nullptr);
    ~InferencePage() override;

protected:
    void modifyOption() override;

private:
    InferencePageWidget *m_widget;
};



#endif //INFERENCEPAGE_H
