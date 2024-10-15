//
// Created by fluty on 24-10-13.
//

#ifndef PARAMEDITORINFOAREA_H
#define PARAMEDITORINFOAREA_H

#include <QWidget>


class QLabel;
class ParamProperties;

class ParamEditorInfoArea final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorInfoArea(QWidget *parent = nullptr);
    void setParamProperties(const ParamProperties &properties);

private:
    const ParamProperties *m_paramProperties = nullptr;
    QLabel *m_lbMax;
    QLabel *m_lbMin;
};



#endif // PARAMEDITORINFOAREA_H
