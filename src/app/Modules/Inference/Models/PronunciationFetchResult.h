#ifndef PRONUNCIATIONFETCHRESULT_H
#define PRONUNCIATIONFETCHRESULT_H

#include <lite/SynthrtEngine/PronunciationStage.h>

#include <QString>
#include <QStringList>

class PronunciationFetchResult {
public:
    QString pronunciation;
    QStringList candidates;
    /// How the language module produced \c pronunciation. Diagnostic only: no surface of the editor
    /// displays it.
    lite::synthrt::PronunciationStage stage = lite::synthrt::PronunciationStage::Unspecified;
};

#endif // PRONUNCIATIONFETCHRESULT_H
