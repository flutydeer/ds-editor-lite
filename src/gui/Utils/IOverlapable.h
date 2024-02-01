//
// Created by fluty on 2024/2/1.
//

#ifndef IOVERLAPABLE_H
#define IOVERLAPABLE_H

class IOverlapable {
public:
    bool isOverlapped() const;
    void setIsOverlapped(bool b);
    virtual bool isOverlappedWith(const IOverlapable &other) = 0;

    virtual ~IOverlapable();

protected:
    bool m_isOverlapped = false;
};

#endif // IOVERLAPABLE_H
