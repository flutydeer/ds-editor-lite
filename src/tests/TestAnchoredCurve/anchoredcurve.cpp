#include "anchoredcurve.h"
#include <cassert>
#include <algorithm>
#include <utility>

template <typename T, typename U>
Knot<T, U>::Knot() : position(T()), value(U()), slope(0.0) {
}

template <typename T, typename U>
Knot<T, U>::Knot(T position, U value, double slope)
    : position(position), value(value), slope(slope) {
}

template <typename T, typename U>
Knot<T, U>::Knot(T position, U value) : position(position), value(value), slope(0.0) {
}

template <typename T, typename U>
Knot<T, U>::Knot(const Knot &other)
    : position(other.position), value(other.value), slope(other.slope) {
}

template <typename T, typename U>
Knot<T, U> &Knot<T, U>::operator=(const Knot &other) {
    if (this != &other) {
        position = other.position;
        value = other.value;
        slope = other.slope;
    }
    return *this;
}

template <typename T, typename U>
T Knot<T, U>::getPosition() const {
    return position;
}

template <typename T, typename U>
void Knot<T, U>::setPosition(T position) {
    this->position = position;
}

template <typename T, typename U>
U Knot<T, U>::getValue() const {
    return value;
}

template <typename T, typename U>
void Knot<T, U>::setValue(U value) {
    this->value = value;
}

template <typename T, typename U>
double Knot<T, U>::getSlope() const {
    return slope;
}

template <typename T, typename U>
void Knot<T, U>::setSlope(double slope) {
    this->slope = slope;
}

template <typename T, typename U>
bool Knot<T, U>::operator==(const Knot &other) const {
    return this->position == other.position;
}

template <typename T, typename U>
Segment<T, U>::Segment() : x_k(T()), x_k1(T()), y_k(U()), y_k1(U()), m_k(0.0), m_k1(0.0) {
}

template <typename T, typename U>
Segment<T, U>::Segment(Knot<T, U> k1, Knot<T, U> k2) {
    x_k = k1.getPosition();
    x_k1 = k2.getPosition();
    y_k = k1.getValue();
    y_k1 = k2.getValue();
    m_k = k1.getSlope();
    m_k1 = k2.getSlope();
}

template <typename T, typename U>
Segment<T, U>::Segment(const Segment &other)
    : x_k(other.x_k), x_k1(other.x_k1), y_k(other.y_k), y_k1(other.y_k1), m_k(other.m_k),
      m_k1(other.m_k1) {
}

template <typename T, typename U>
Segment<T, U> &Segment<T, U>::operator=(const Segment &other) {
    if (this != &other) {
        x_k = other.x_k;
        x_k1 = other.x_k1;
        y_k = other.y_k;
        y_k1 = other.y_k1;
        m_k = other.m_k;
        m_k1 = other.m_k1;
    }
    return *this;
}

template <typename T, typename U>
U Segment<T, U>::getValue(T position) const {
    assert((x_k <= position) && (position <= x_k1));
    T x_k1_minus_x_k = x_k1 - x_k;
    T x_minus_x_k = position - x_k;
    T x_minus_x_k1 = position - x_k1;
    double a = x_minus_x_k / x_k1_minus_x_k;
    double b = -x_minus_x_k1 / x_k1_minus_x_k;
    U res = (1 + 2 * a) * b * b * y_k + (1 + 2 * b) * a * a * y_k1 + x_minus_x_k * b * b * m_k +
            x_minus_x_k1 * a * a * m_k1;
    return res;
}

template <typename T, typename U>
AnchoredCurve<T, U>::AnchoredCurve() {
    knots.clear();
    segments.clear();
}

template <typename T, typename U>
AnchoredCurve<T, U>::~AnchoredCurve() = default;

template <typename T, typename U>
AnchoredCurve<T, U>::AnchoredCurve(const AnchoredCurve &other) {
    knots = other.knots;
    segments = other.segments;
}

template <typename T, typename U>
AnchoredCurve<T, U> &AnchoredCurve<T, U>::operator=(const AnchoredCurve &other) {
    if (this != &other) {
        knots = other.knots;
        segments = other.segments;
    }
    return *this;
}

template <typename T, typename U>
AnchoredCurve<T, U>::AnchoredCurve(const QList<Knot<T, U>> &knots) {
    this->knots = knots;
    this->updateAll();
}

template <typename T, typename U>
AnchoredCurve<T, U>::AnchoredCurve(std::initializer_list<T> positions,
                                   std::initializer_list<U> values) {
    // 使用 std::initializer_list 的 size() 成员函数
    for (size_t i = 0; i < positions.size(); ++i) {
        knots.push_back(Knot<T, U>(*(positions.begin() + i), *(values.begin() + i)));
    }
    updateAll();
}

template <typename T, typename U>
U AnchoredCurve<T, U>::getValue(const T position) const {
    if (segments.count() <= 0) {
        return U();
    }
    auto it =
        std::lower_bound(knots.begin(), knots.end(), Knot<T, U>(position, U()), knotCmp<T, U>);
    if (it == knots.end()) {
        return knots.back().getValue();
    }
    if (it == knots.begin()) {
        return knots.front().getValue();
    }
    auto index = std::distance(knots.begin(), it);
    return segments[index - 1].getValue(position);
}

template <typename T, typename U>
std::vector<std::pair<T, U>> AnchoredCurve<T, U>::getValueLinspace(const T start, const T end,
                                                                   const int num) const {
    std::vector<std::pair<T, U>> res;
    if (num == 0) {
        return res;
    }
    auto knot_it =
        std::lower_bound(knots.begin(), knots.end(), Knot<T, U>(start, 0), knotCmp<T, U>);
    int curr_segment_index = std::distance(knots.begin(), knot_it) - 1;
    double num_d = num;
    for (int i = 0; i < num; i++) {
        double w = (static_cast<double>(i) / num_d);
        T position = static_cast<T>((1.0 - w) * start + w * end);

        if (curr_segment_index < 0) {
            if (knots.front().getPosition() <= position) {
                curr_segment_index = 0;
            }
        } else if (curr_segment_index >= segments.count()) {

        } else {
            if (knots[curr_segment_index + 1].getPosition() <= position) {
                ++curr_segment_index;
            }
        }

        if (curr_segment_index < 0) {
            res.push_back(std::make_pair(position, knots.front().getValue()));
        } else if (curr_segment_index >= segments.count()) {
            res.push_back(std::make_pair(position, knots.back().getValue()));
        } else {
            res.push_back(
                std::make_pair(position, segments[curr_segment_index].getValue(position)));
        }
    }
    return res;
}

template <typename T, typename U>
void AnchoredCurve<T, U>::insert(const Knot<T, U> knot) {
    // 边界条件，knots.count()==0
    if (knots.count() <= 0) {
        knots.push_back(knot);
        return;
    }

    auto it = std::lower_bound(knots.begin(), knots.end(), knot, knotCmp<T, U>);
    int index;
    if (it != knots.end()) {
        index = std::distance(knots.begin(), it);
    } else {
        index = knots.count();
    }

    if (index >= knots.count()) {
        knots.push_back(knot);
        segments.push_back(Segment<T, U>());
    } else {
        if (!(knots[index] == knot)) {
            knots.insert(index, knot);
            if (segments.count() <= 0) {
                segments.push_back(Segment<T, U>());
            } else {
                segments.insert(index, Segment<T, U>());
            }
        } else {
            knots[index] = knot;
        }
    }
    this->update(index - 1, index + 1);
}

template <typename T, typename U>
void AnchoredCurve<T, U>::insert(T position, U value) {
    insert(Knot<T, U>(position, value));
}

template <typename T, typename U>
void AnchoredCurve<T, U>::merge(const QList<Knot<T, U>> &knots) {
    this->knots = this->knots + knots;
    this->updateAll();
}

template <typename T, typename U>
void AnchoredCurve<T, U>::merge(const AnchoredCurve &other) {
    merge(other.getKnots());
}

template <typename T, typename U>
void AnchoredCurve<T, U>::merge(std::initializer_list<T> positions,
                                std::initializer_list<U> values) {
    QList<Knot<T, U>> knots_new;
    for (size_t i = 0; i < positions.size(); ++i) {
        knots_new.push_back(Knot<T, U>(*(positions.begin() + i), *(values.begin() + i)));
    }
    merge(knots_new);
}

template <typename T, typename U>
void AnchoredCurve<T, U>::remove(T x) {
    // 边界条件，knots.count()==0
    if (knots.count() <= 0) {
        return;
    }

    auto it = std::lower_bound(knots.begin(), knots.end(), Knot<T, U>(x, U()), knotCmp<T, U>);
    if (it == knots.end()) {
        return;
    } else if (it->getPosition() == x) {
        int index = std::distance(knots.begin(), it);
        knots.removeAt(index);
        if (index < segments.count()) {
            segments.removeAt(index);
        } else {
            segments.removeLast();
        }
        update(index - 1, index);
    }
}


template <typename T, typename U>
void AnchoredCurve<T, U>::removeRange(T l, T r) {
    auto start_it = std::lower_bound(knots.begin(), knots.end(), Knot<T, U>(l, U()), knotCmp<T, U>);
    auto end_it = std::upper_bound(knots.begin(), knots.end(), Knot<T, U>(r, U()), knotCmp<T, U>);
    int start_index = std::distance(knots.begin(), start_it);
    int end_index = std::distance(knots.begin(), end_it);
    knots.erase(start_it, end_it);
    segments.erase(segments.begin() + start_index, segments.begin() + end_index);
    update(start_index - 1, start_index);
}

template <typename T, typename U>
const QList<Knot<T, U>> &AnchoredCurve<T, U>::getKnots() const {
    return knots;
}

template <typename T, typename U>
const QList<Segment<T, U>> &AnchoredCurve<T, U>::getSegments() const {
    return segments;
}

template <typename T, typename U>
double AnchoredCurve<T, U>::getInteriorSlope(int index) const {
    U delta_y_l = (knots[index].getValue() - knots[index - 1].getValue());
    U delta_y_r = (knots[index + 1].getValue() - knots[index].getValue());
    if ((delta_y_l * delta_y_r) <= 0) {
        return 0;
    }
    T delta_x_l = (knots[index].getPosition() - knots[index - 1].getPosition());
    T delta_x_r = (knots[index + 1].getPosition() - knots[index].getPosition());
    double slope_l = delta_y_l / delta_x_l;
    double slope_r = delta_y_r / delta_x_r;
    double w_l = delta_x_l + 2 * delta_x_r, w_r = 2 * delta_x_l + delta_x_r;
    return (w_l + w_r) / (w_l / slope_l + w_r / slope_r);
}

template <typename T, typename U>
void AnchoredCurve<T, U>::updateAll() {
    if (knots.count() <= 1) {
        return;
    }
    this->updateAllKnots();
    this->updateAllSegments();
}

template <typename T, typename U>
void AnchoredCurve<T, U>::updateAllKnots() {
    int len = knots.count();

    std::sort(knots.begin(), knots.end(), knotCmp<T, U>);
    double slope_start = static_cast<double>(knots[1].getValue() - knots.front().getValue()) /
                         (knots[1].getPosition() - knots.front().getPosition());
    knots.front().setSlope(slope_start);
    double slope_end = static_cast<double>(knots.back().getValue() - knots[len - 2].getValue()) /
                       (knots.back().getPosition() - knots[len - 2].getPosition());
    knots.back().setSlope(slope_end);
    if (len <= 2) {
        return;
    }

    for (int i = 1; i < len - 1; i++) {
        knots[i].setSlope(getInteriorSlope(i));
    }
}

template <typename T, typename U>
void AnchoredCurve<T, U>::updateAllSegments() {
    segments.clear();
    for (int i = 0; i < knots.count() - 1; i++) {
        segments.push_back(Segment<T, U>(knots.at(i), knots.at(i + 1)));
    }
}

template <typename T, typename U>
void AnchoredCurve<T, U>::update(int start_index, int end_index) {
    if (knots.count() <= 1) {
        return;
    }
    this->updateKnots(start_index, end_index);
    this->updateSegments(start_index, end_index);
}


template <typename T, typename U>
void AnchoredCurve<T, U>::updateKnots(int start_index, int end_index) {
    int len = knots.count();

    for (int i = start_index; i <= end_index; i++) {
        if (i < 0 || i >= len) {
            continue;
        }
        if (i == 0) {
            knots[i].setSlope((knots[1].getValue() - knots.front().getValue()) /
                              (knots[1].getPosition() - knots.front().getPosition()));
        } else if (i == len - 1) {
            knots[i].setSlope((knots.back().getValue() - knots[len - 2].getValue()) /
                              (knots.back().getPosition() - knots[len - 2].getPosition()));
        } else {
            knots[i].setSlope(getInteriorSlope(i));
        }
    }
}


template <typename T, typename U>
void AnchoredCurve<T, U>::updateSegments(int start_index, int end_index) {
    for (int i = start_index - 1; i < end_index + 1; i++) {
        if (i < 0 || i >= segments.count()) {
            continue;
        }
        segments[i] = Segment<T, U>(knots[i], knots[i + 1]);
    }
}
