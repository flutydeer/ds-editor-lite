#ifndef ANCHOREDCURVE_H
#define ANCHOREDCURVE_H

#include <vector>
#include <utility>
#include <QList>

template <typename TPos, typename TValue>
class Knot {
public:
    Knot();
    Knot(TPos position, TValue value);
    Knot(TPos position, TValue value, double slope);
    Knot(const Knot &other);
    Knot &operator=(const Knot &other);

    TPos getPosition() const;
    void setPosition(TPos position);
    TValue getValue() const;
    void setValue(TValue value);
    [[nodiscard]] double getSlope() const;
    void setSlope(double slope);

    bool operator==(const Knot &other) const;

private:
    TPos position;
    TValue value;
    double slope;
};

template <typename TPos, typename TValue>
bool knotCmp(Knot<TPos, TValue> a, Knot<TPos, TValue> b) {
    return a.getPosition() < b.getPosition();
}

template <typename TPos, typename TValue>
class Segment {
public:
    Segment();
    Segment(Knot<TPos, TValue> k1, Knot<TPos, TValue> k2);
    Segment(const Segment &other);
    Segment &operator=(const Segment &other);

    TValue getValue(TPos position) const;

private:
    TPos x_k;
    TPos x_k1;
    TValue y_k;
    TValue y_k1;
    double m_k;
    double m_k1;
};

template <typename TPos, typename TValue>
class AnchoredCurve {
public:
    AnchoredCurve();
    ~AnchoredCurve();
    AnchoredCurve(const AnchoredCurve &other);
    AnchoredCurve &operator=(const AnchoredCurve &other);
    explicit AnchoredCurve(const QList<Knot<TPos, TValue>> &knots);
    AnchoredCurve(std::initializer_list<TPos> positions, std::initializer_list<TValue> values);

    TValue getValue(TPos position) const;
    std::vector<std::pair<TPos, TValue>> getValueLinspace(TPos start, TPos end, int num) const;
    void insert(Knot<TPos, TValue> knot);
    void insert(TPos position, TValue value);
    void merge(const QList<Knot<TPos, TValue>> &knots);
    void merge(const AnchoredCurve &other);
    void merge(std::initializer_list<TPos> positions, std::initializer_list<TValue> values);
    void remove(TPos x);
    void removeRange(TPos l, TPos r); // 移除闭区间[l,r]

    const QList<Knot<TPos, TValue>> &getKnots() const;
    const QList<Segment<TPos, TValue>> &getSegments() const;

private:
    QList<Knot<TPos, TValue>> knots;
    QList<Segment<TPos, TValue>> segments;

    [[nodiscard]] double getInteriorSlope(int index) const;

    void updateAll();
    void updateAllKnots();
    void updateAllSegments();

    void update(int start_index, int end_index); // 更新闭区间[start_index,end_index]
    void updateKnots(int start_index, int end_index);
    void updateSegments(int start_index, int end_index);
};

#endif // ANCHOREDCURVE_H
