#include "anchoredcurve.h"
#include <cassert>
#include <algorithm>
#include <utility>

template class AnchoredCurve<int, double>;
template class Knot<int, double>;

template <typename TPos, typename TValue>
Knot<TPos, TValue>::Knot() : position(TPos()), value(TValue()), slope(0.0) {
}

template <typename TPos, typename TValue>
Knot<TPos, TValue>::Knot(TPos position, TValue value, double slope)
    : position(position), value(value), slope(slope) {
}

template <typename TPos, typename TValue>
Knot<TPos, TValue>::Knot(TPos position, TValue value)
    : position(position), value(value), slope(0.0) {
}

template <typename TPos, typename TValue>
Knot<TPos, TValue>::Knot(const Knot &other)
    : position(other.position), value(other.value), slope(other.slope) {
}

template <typename TPos, typename TValue>
Knot<TPos, TValue> &Knot<TPos, TValue>::operator=(const Knot &other) {
    if (this != &other) {
        position = other.position;
        value = other.value;
        slope = other.slope;
    }
    return *this;
}

template <typename TPos, typename TValue>
TPos Knot<TPos, TValue>::getPosition() const {
    return position;
}

template <typename TPos, typename TValue>
void Knot<TPos, TValue>::setPosition(TPos position) {
    this->position = position;
}

template <typename TPos, typename TValue>
TValue Knot<TPos, TValue>::getValue() const {
    return value;
}

template <typename TPos, typename TValue>
void Knot<TPos, TValue>::setValue(TValue value) {
    this->value = value;
}

template <typename TPos, typename TValue>
double Knot<TPos, TValue>::getSlope() const {
    return slope;
}

template <typename TPos, typename TValue>
void Knot<TPos, TValue>::setSlope(double slope) {
    this->slope = slope;
}

template <typename TPos, typename TValue>
bool Knot<TPos, TValue>::operator==(const Knot &other) const {
    return this->position == other.position;
}

template <typename TPos, typename TValue>
Segment<TPos, TValue>::Segment()
    : x_k(TPos()), x_k1(TPos()), y_k(TValue()), y_k1(TValue()), m_k(0.0), m_k1(0.0) {
}

template <typename TPos, typename TValue>
Segment<TPos, TValue>::Segment(Knot<TPos, TValue> k1, Knot<TPos, TValue> k2) {
    x_k = k1.getPosition();
    x_k1 = k2.getPosition();
    y_k = k1.getValue();
    y_k1 = k2.getValue();
    m_k = k1.getSlope();
    m_k1 = k2.getSlope();
}

template <typename TPos, typename TValue>
Segment<TPos, TValue>::Segment(const Segment &other)
    : x_k(other.x_k), x_k1(other.x_k1), y_k(other.y_k), y_k1(other.y_k1), m_k(other.m_k),
      m_k1(other.m_k1) {
}

template <typename TPos, typename TValue>
Segment<TPos, TValue> &Segment<TPos, TValue>::operator=(const Segment &other) {
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

template <typename TPos, typename TValue>
TValue Segment<TPos, TValue>::getValue(TPos position) const {
    assert((x_k <= position) && (position <= x_k1));
    TPos x_k1_minus_x_k = x_k1 - x_k;
    TPos x_minus_x_k = position - x_k;
    TPos x_minus_x_k1 = position - x_k1;
    double a = 1.0 * x_minus_x_k / x_k1_minus_x_k;
    double b = 1.0 * -x_minus_x_k1 / x_k1_minus_x_k;
    TValue res = (1 + 2 * a) * b * b * y_k + (1 + 2 * b) * a * a * y_k1 +
                 x_minus_x_k * b * b * m_k + x_minus_x_k1 * a * a * m_k1;
    return res;
}

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue>::AnchoredCurve() {
    knots.clear();
    segments.clear();
}

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue>::~AnchoredCurve() = default;

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue>::AnchoredCurve(const AnchoredCurve &other) {
    knots = other.knots;
    segments = other.segments;
}

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue> &AnchoredCurve<TPos, TValue>::operator=(const AnchoredCurve &other) {
    if (this != &other) {
        knots = other.knots;
        segments = other.segments;
    }
    return *this;
}

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue>::AnchoredCurve(const QList<Knot<TPos, TValue>> &knots) {
    this->knots = knots;
    this->updateAll();
}

template <typename TPos, typename TValue>
AnchoredCurve<TPos, TValue>::AnchoredCurve(std::initializer_list<TPos> positions,
                                           std::initializer_list<TValue> values) {
    // 使用 std::initializer_list 的 size() 成员函数
    for (size_t i = 0; i < positions.size(); ++i) {
        knots.push_back(Knot<TPos, TValue>(*(positions.begin() + i), *(values.begin() + i)));
    }
    updateAll();
}

template <typename TPos, typename TValue>
TValue AnchoredCurve<TPos, TValue>::getValue(const TPos position) const {
    if (segments.count() <= 0) {
        return TValue();
    }
    auto it = std::lower_bound(knots.begin(), knots.end(), Knot<TPos, TValue>(position, TValue()),
                               knotCmp<TPos, TValue>);
    if (it == knots.end()) {
        return knots.back().getValue();
    }
    if (it == knots.begin()) {
        return knots.front().getValue();
    }
    auto index = std::distance(knots.begin(), it);
    return segments[index - 1].getValue(position);
}

template <typename TPos, typename TValue>
std::vector<std::pair<TPos, TValue>>
    AnchoredCurve<TPos, TValue>::getValueLinspace(const TPos start, const TPos end,
                                                  const int num) const {
    std::vector<std::pair<TPos, TValue>> res;
    if (num == 0) {
        return res;
    }
    auto knot_it = std::lower_bound(knots.begin(), knots.end(), Knot<TPos, TValue>(start, 0),
                                    knotCmp<TPos, TValue>);
    int curr_segment_index = std::distance(knots.begin(), knot_it) - 1;
    double num_d = num;
    for (int i = 0; i < num; i++) {
        double w = (static_cast<double>(i) / num_d);
        TPos position = static_cast<TPos>((1.0 - w) * start + w * end);

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

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::insert(const Knot<TPos, TValue> knot) {
    // 边界条件，knots.count()==0
    if (knots.count() <= 0) {
        knots.push_back(knot);
        return;
    }

    auto it = std::lower_bound(knots.begin(), knots.end(), knot, knotCmp<TPos, TValue>);
    int index;
    if (it != knots.end()) {
        index = std::distance(knots.begin(), it);
    } else {
        index = knots.count();
    }

    if (index >= knots.count()) {
        knots.push_back(knot);
        segments.push_back(Segment<TPos, TValue>());
    } else {
        if (!(knots[index] == knot)) {
            knots.insert(index, knot);
            if (segments.count() <= 0) {
                segments.push_back(Segment<TPos, TValue>());
            } else {
                segments.insert(index, Segment<TPos, TValue>());
            }
        } else {
            knots[index] = knot;
        }
    }
    this->update(index - 1, index + 1);
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::insert(TPos position, TValue value) {
    insert(Knot<TPos, TValue>(position, value));
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::merge(const QList<Knot<TPos, TValue>> &knots) {
    this->knots = this->knots + knots;
    this->updateAll();
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::merge(const AnchoredCurve &other) {
    merge(other.getKnots());
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::merge(std::initializer_list<TPos> positions,
                                        std::initializer_list<TValue> values) {
    QList<Knot<TPos, TValue>> knots_new;
    for (size_t i = 0; i < positions.size(); ++i) {
        knots_new.push_back(Knot<TPos, TValue>(*(positions.begin() + i), *(values.begin() + i)));
    }
    merge(knots_new);
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::remove(TPos x) {
    // 边界条件，knots.count()==0
    if (knots.count() <= 0) {
        return;
    }

    auto it = std::lower_bound(knots.begin(), knots.end(), Knot<TPos, TValue>(x, TValue()),
                               knotCmp<TPos, TValue>);
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

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::removeRange(TPos l, TPos r) {
    auto start_it = std::lower_bound(knots.begin(), knots.end(), Knot<TPos, TValue>(l, TValue()),
                                     knotCmp<TPos, TValue>);
    auto end_it = std::upper_bound(knots.begin(), knots.end(), Knot<TPos, TValue>(r, TValue()),
                                   knotCmp<TPos, TValue>);
    int start_index = std::distance(knots.begin(), start_it);
    int end_index = std::distance(knots.begin(), end_it);
    knots.erase(start_it, end_it);
    segments.erase(segments.begin() + start_index, segments.begin() + end_index);
    update(start_index - 1, start_index);
}

template <typename TPos, typename TValue>
const QList<Knot<TPos, TValue>> &AnchoredCurve<TPos, TValue>::getKnots() const {
    return knots;
}

template <typename TPos, typename TValue>
const QList<Segment<TPos, TValue>> &AnchoredCurve<TPos, TValue>::getSegments() const {
    return segments;
}

template <typename TPos, typename TValue>
double AnchoredCurve<TPos, TValue>::getInteriorSlope(int index) const {
    TValue delta_y_l = (knots[index].getValue() - knots[index - 1].getValue());
    TValue delta_y_r = (knots[index + 1].getValue() - knots[index].getValue());
    if ((delta_y_l * delta_y_r) <= 0) {
        return 0;
    }
    TPos delta_x_l = (knots[index].getPosition() - knots[index - 1].getPosition());
    TPos delta_x_r = (knots[index + 1].getPosition() - knots[index].getPosition());
    double slope_l = delta_y_l / delta_x_l;
    double slope_r = delta_y_r / delta_x_r;
    double w_l = delta_x_l + 2 * delta_x_r, w_r = 2 * delta_x_l + delta_x_r;
    return (w_l + w_r) / (w_l / slope_l + w_r / slope_r);
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::updateAll() {
    if (knots.count() <= 1) {
        return;
    }
    this->updateAllKnots();
    this->updateAllSegments();
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::updateAllKnots() {
    int len = knots.count();

    std::sort(knots.begin(), knots.end(), knotCmp<TPos, TValue>);
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

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::updateAllSegments() {
    segments.clear();
    for (int i = 0; i < knots.count() - 1; i++) {
        segments.push_back(Segment<TPos, TValue>(knots.at(i), knots.at(i + 1)));
    }
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::update(int start_index, int end_index) {
    if (knots.count() <= 1) {
        return;
    }
    this->updateKnots(start_index, end_index);
    this->updateSegments(start_index, end_index);
}

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::updateKnots(int start_index, int end_index) {
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

template <typename TPos, typename TValue>
void AnchoredCurve<TPos, TValue>::updateSegments(int start_index, int end_index) {
    for (int i = start_index - 1; i < end_index + 1; i++) {
        if (i < 0 || i >= segments.count()) {
            continue;
        }
        segments[i] = Segment<TPos, TValue>(knots[i], knots[i + 1]);
    }
}
