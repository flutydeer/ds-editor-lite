//
// Created by fluty on 2024/2/1.
//

#ifndef ICOMPARABLE_H
#define ICOMPARABLE_H

class IComparable {
public:
    virtual int compareTo(IComparable *other) = 0;

    virtual ~IComparable() = default;
};

#endif // ICOMPARABLE_H
