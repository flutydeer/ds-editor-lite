#ifndef DS_EDITOR_LITE_DESCRIPTIONCARD_H
#define DS_EDITOR_LITE_DESCRIPTIONCARD_H

#include <lite/GUI/Controls/OptionsCard.h>

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