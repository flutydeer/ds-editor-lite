//
// Created by fluty on 2024/1/31.
//

#ifndef SINGLETON_H
#define SINGLETON_H

template <typename T>
class Singleton {
public:
    Singleton() = default;
    virtual ~Singleton() = default;
    static T *instance() {
        static T obj;
        return &obj;
    }
};

#endif // SINGLETON_H
