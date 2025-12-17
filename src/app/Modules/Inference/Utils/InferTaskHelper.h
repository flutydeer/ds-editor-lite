//
// Created by fluty on 24-10-2.
//

#ifndef INFERTASKHELPER_H
#define INFERTASKHELPER_H

#include <QList>

class InferInputBase;
class InferWord;

class InferTaskHelper {
public:
    static QList<InferWord> buildWords(const InferInputBase &input, bool useOffsetInfo = false);
};

#endif // INFERTASKHELPER_H
