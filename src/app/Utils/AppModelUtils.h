//
// Created by OrangeCat on 24-9-4.
//

#ifndef APPMODELUTILS_H
#define APPMODELUTILS_H
#include <QList>

class Note;

class AppModelUtils {
public:
    static void copyNotes(const QList<Note *> &source, QList<Note *> &target);
    static QList<QList<Note *>> simpleSegment(const QList<Note *> &source, double threshold = 800);
};



#endif //APPMODELUTILS_H
