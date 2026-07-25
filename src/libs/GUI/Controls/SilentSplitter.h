//
// Created by fluty on 2026/5/5.
//

#ifndef SILENTSPLITTER_H
#define SILENTSPLITTER_H

#include <QSplitter>
#include <QSplitterHandle>

class SilentHandle : public QSplitterHandle {
public:
    using QSplitterHandle::QSplitterHandle;

protected:
    void paintEvent(QPaintEvent *event) override {}
};

class SilentSplitter : public QSplitter {
public:
    using QSplitter::QSplitter;

protected:
    QSplitterHandle *createHandle() override {
        return new SilentHandle(orientation(), this);
    }
};

#endif // SILENTSPLITTER_H
