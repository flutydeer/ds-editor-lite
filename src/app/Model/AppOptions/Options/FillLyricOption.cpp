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

    if (object.contains("exportLanguage"))
        exportLanguage = object["exportLanguage"].toBool();

    if (object.contains("textEditFontSize"))
        textEditFontSize = object["textEditFontSize"].toDouble();
    if (object.contains("viewFontSize"))
        viewFontSize = object["viewFontSize"].toDouble();
}

void FillLyricOption::save(QJsonObject &object) {
    object["baseVisible"] = baseVisible;
    object["extVisible"] = extVisible;

    object["splitMode"] = splitMode;
    object["skipSlur"] = skipSlur;

    object["autoWrap"] = autoWrap;

    object["exportLanguage"] = exportLanguage;

    object["textEditFontSize"] = textEditFontSize;
    object["viewFontSize"] = viewFontSize;
}