#ifndef ANCHOREDCURVE_H
#define ANCHOREDCURVE_H

#include <vector>
#include <utility>
#include <QList>

template <typename T, typename U>
class Knot {
public:
    Knot();
    Knot(T position, U value);
    Knot(T position, U value, double slope);
    Knot(const Knot &other);
    Knot &operator=(const Knot &other);

    const T getPosition() const;
    void setPosition(T position);
    const U getValue() const;
    void setValue(U value);
    const double getSlope() const;
    void setSlope(double slope);

    const bool operator==(const Knot &other) const;

private:
    T position;
    U value;
    double slope;
};

template <typename T, typename U>
bool knotCmp(Knot<T, U> a, Knot<T, U> b) {
    return a.getPosition() < b.getPosition();
}

template <typename T, typename U>
class Segment {
public:
    Segment();
    Segment(Knot<T, U> k1, Knot<T, U> k2);
    Segment(const Segment &other);
    Segment &operator=(const Segment &other);

    const U getValue(T position) const;

private:
    T x_k;
    T x_k1;
    U y_k;
    U y_k1;
    double m_k;
    double m_k1;
};

template <typename T, typename U>
class AnchoredCurve {
public:
    AnchoredCurve();
    ~AnchoredCurve();
    AnchoredCurve(const AnchoredCurve &other);
    AnchoredCurve &operator=(const AnchoredCurve &other);
    AnchoredCurve(const QList<Knot<T, U>> knots);
    AnchoredCurve(std::initializer_list<T> positions, std::initializer_list<U> values);

    const U getValue(const T position) const;
    const std::vector<std::pair<T, U>> getValueLinspace(const T start, const T end, const int num);
    void insert(const Knot<T, U> knot);
    void insert(T position, U value);
    void merge(const QList<Knot<T, U>> knots);
    void merge(const AnchoredCurve &other);
    void merge(std::initializer_list<T> positions, std::initializer_list<U> values);
    void remove(T x);
    void removeRange(T l, T r); // 移除闭区间[l,r]

    const QList<Knot<T, U>> getKnots() const;
    const QList<Segment<T, U>> getSegments() const;

private:
    QList<Knot<T, U>> knots;
    QList<Segment<T, U>> segments;

    double getInteriorSlope(int index) const;

    void updateAll();
    void updateAllKnots();
    void updateAllSegments();

    void update(int start_index, int end_index); // 更新闭区间[start_index,end_index]
    void updateKnots(int start_index, int end_index);
    void updateSegments(int start_index, int end_index);
};

#endif // ANCHOREDCURVE_H
