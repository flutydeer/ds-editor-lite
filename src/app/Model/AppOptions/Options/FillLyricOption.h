#ifndef FILLLYRICOPTION_H
#define FILLLYRICOPTION_H

#include "Model/AppOptions/IOption.h"

class FillLyricOption final : public IOption {
public:
    explicit FillLyricOption() : IOption("fillLyric") {
    }

    void load(const QJsonObject &object) override;

    bool baseVisible = true;
    bool extVisible = false;

    int splitMode = 0;
    bool skipSlur = false;

    bool autoWrap = false;

    bool exportSkipSlur = false;
    bool exportLanguage = false;

    double textEditFontSize = 11;
    int tableFontSize = 12;

    double tableColWidthRatio = 7.8;
    double tableRowHeightRatio = 3;
    int tableFontDiff = 3;

protected:
    void save(QJsonObject &object) override;
};



#endif // FILLLYRICOPTION_H
