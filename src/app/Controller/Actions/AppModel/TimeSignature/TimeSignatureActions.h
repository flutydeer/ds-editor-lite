#ifndef TIMESIGNATUREACTIONS_H
#define TIMESIGNATUREACTIONS_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/History/ActionSequence.h>

class TimeSignatureActions : public ActionSequence {
public:
    // Inserts a point at signature.barIndex, or replaces the values of the
    // existing point at that bar
    void setTimeSignatureAt(const TimeSignature &signature, AppModel *model);
    // Removes the point at exactly barIndex; callers must not pass bar 0
    void removeTimeSignatureAt(int barIndex, AppModel *model);
};



#endif // TIMESIGNATUREACTIONS_H
