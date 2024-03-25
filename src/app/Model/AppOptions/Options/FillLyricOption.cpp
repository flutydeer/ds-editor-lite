#include "FillLyricOption.h"

void FillLyricOption::load(const QJsonObject &object) {
    if (object.contains("baseVisible"))
        baseVisible = object["baseVisible"].toBool();
    if (object.contains("extVisible"))
        extVisible = object["extVisible"].toBool();

    if (object.contains("splitMode"))
        splitMode = object["splitMode"].toInt();
    if (object.contains("skipSlur"))
        skipSlur = object["skipSlur"].toBool();

    if (object.contains("autoWrap"))
        autoWrap = object["autoWrap"].toBool();

    if (object.contains("exportSkipSlur"))
        exportSkipSlur = object["exportSkipSlur"].toBool();
    if (object.contains("exportSkipEndSpace"))
        exportSkipEndSpace = object["exportSkipEndSpace"].toBool();
    if (object.contains("exportLanguage"))
        exportLanguage = object["exportLanguage"].toBool();

    if (object.contains("textEditFontSize"))
        textEditFontSize = object["textEditFontSize"].toDouble();
    if (object.contains("tableFontSize"))
        tableFontSize = object["tableFontSize"].toInt();

    if (object.contains("tableColWidthRatio"))
        tableColWidthRatio = object["tableColWidthRatio"].toDouble();
    if (object.contains("tableRowHeightRatio"))
        tableRowHeightRatio = object["tableRowHeightRatio"].toDouble();
    if (object.contains("tableFontDiff"))
        tableFontDiff = object["tableFontDiff"].toInt();
}

void FillLyricOption::save(QJsonObject &object) {
    object["baseVisible"] = baseVisible;
    object["extVisible"] = extVisible;

    object["splitMode"] = splitMode;
    object["skipSlur"] = skipSlur;

    object["autoWrap"] = autoWrap;

    object["exportSkipSlur"] = exportSkipSlur;
    object["exportSkipEndSpace"] = exportSkipEndSpace;
    object["exportLanguage"] = exportLanguage;

    object["textEditFontSize"] = textEditFontSize;
    object["tableFontSize"] = tableFontSize;

    object["tableColWidthRatio"] = tableColWidthRatio;
    object["tableRowHeightRatio"] = tableRowHeightRatio;
    object["tableFontDiff"] = tableFontDiff;
}