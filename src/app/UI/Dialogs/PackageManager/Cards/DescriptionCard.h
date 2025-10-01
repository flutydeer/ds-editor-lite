//
// Created by FlutyDeer on 2025/9/3.
//

#ifndef DS_EDITOR_LITE_DESCRIPTIONCARD_H
#define DS_EDITOR_LITE_DESCRIPTIONCARD_H

#include "UI/Controls/OptionsCard.h"

class QPlainTextEdit;

class DescriptionCard : public OptionsCard {
    Q_OBJECT

public:
    explicit DescriptionCard(QWidget *parent = nullptr);

public slots:
    void onDataContextChanged(const QString &dataContext);

private:
    // QPlainTextEdit *plainTextEdit  = nullptr;
    QLabel *lbDescription = nullptr;
};


#endif // DS_EDITOR_LITE_DESCRIPTIONCARD_H