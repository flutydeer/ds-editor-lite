#include "TimeSignatureActions.h"

#include "EditTimeSignaturesAction.h"

#include <algorithm>

void TimeSignatureActions::setTimeSignatureAt(const TimeSignature &signature, AppModel *model) {
    const auto &oldSignatures = model->timeline().timeSignatures();
    const bool exists = std::any_of(
        oldSignatures.cbegin(), oldSignatures.cend(),
        [&](const TimeSignature &existing) { return existing.barIndex == signature.barIndex; });
    QList<TimeSignature> newSignatures;
    for (const auto &existing : oldSignatures) {
        if (existing.barIndex != signature.barIndex)
            newSignatures.append(existing);
    }
    newSignatures.append(signature);
    std::sort(
        newSignatures.begin(), newSignatures.end(),
        [](const TimeSignature &a, const TimeSignature &b) { return a.barIndex < b.barIndex; });

    setTranslatableName("TimeSignatureActions",
                        exists
                            ? QT_TRANSLATE_NOOP("TimeSignatureActions", "Edit Time Signature")
                            : QT_TRANSLATE_NOOP("TimeSignatureActions", "Insert Time Signature"));
    addAction(EditTimeSignaturesAction::build(oldSignatures, newSignatures, model));
}

void TimeSignatureActions::removeTimeSignatureAt(const int barIndex, AppModel *model) {
    const auto &oldSignatures = model->timeline().timeSignatures();
    QList<TimeSignature> newSignatures;
    for (const auto &existing : oldSignatures) {
        if (existing.barIndex != barIndex)
            newSignatures.append(existing);
    }

    setTranslatableName("TimeSignatureActions",
                        QT_TRANSLATE_NOOP("TimeSignatureActions", "Remove Time Signature"));
    addAction(EditTimeSignaturesAction::build(oldSignatures, newSignatures, model));
}
