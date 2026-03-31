//
// Created by FlutyDeer on 2026/4/1.
//

#ifndef DS_EDITOR_LITE_PHONEMENAMEITEMMODEL_H
#define DS_EDITOR_LITE_PHONEMENAMEITEMMODEL_H

#include <QString>

class PhonemeNameItemModel {
public:
    PhonemeNameItemModel() = default;
    PhonemeNameItemModel(const QString &language, const QString &name, bool isOnset)
        : m_language(language), m_name(name), m_isOnset(isOnset) {}

    QString language() const { return m_language; }
    void setLanguage(const QString &language) { m_language = language; }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    bool isOnset() const { return m_isOnset; }
    void setIsOnset(bool isOnset) { m_isOnset = isOnset; }

private:
    QString m_language;
    QString m_name;
    bool m_isOnset = false;
};



#endif //DS_EDITOR_LITE_PHONEMENAMEITEMMODEL_H
