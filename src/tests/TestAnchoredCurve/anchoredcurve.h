#ifndef ANCHOREDCURVE_H
#define ANCHOREDCURVE_H

#include <vector>
#include <utility>
#include <QList>

template <typename TPos, typename TValue>
class Knot {
public:
    Knot() : _position(TPos()), _value(TValue()), _slope(0.0) {
    }

    Knot(TPos position, TValue value) : _position(position), _value(value), _slope(0.0) {
    }

    Knot(TPos position, TValue value, double slope)
        : _position(position), _value(value), _slope(slope) {
    }

    Knot(const Knot &other)
        : _position(other._position), _value(other._value), _slope(other._slope) {
    }

    Knot &operator=(const Knot &other) {
        if (this != &other) {
            _position = other._position;
            _value = other._value;
            _slope = other._slope;
        }
        return *this;
    }

    TPos getPosition() const {
        return _position;
    }

    void setPosition(TPos position) {
        _position = position;
    }

    TValue getValue() const {
        return _value;
    }

    void setValue(TValue value) {
        _value = value;
    }

    [[nodiscard]] double getSlope() const {
        return _slope;
    }

    void setSlope(double slope) {
        _slope = slope;
    }

    bool operator==(const Knot &other) const {
        return _position == other._position;
    }

private:
    TPos _position;
    TValue _value;
    double _slope;
};

template <typename TPos, typename TValue>
bool knotCmp(Knot<TPos, TValue> a, Knot<TPos, TValue> b) {
    return a.getPosition() < b.getPosition();
}

template <typename TPos, typename TValue>
class Segment {
public:
    Segment() : x_k(TPos()), x_k1(TPos()), y_k(TValue()), y_k1(TValue()), m_k(0.0), m_k1(0.0) {
    }

    Segment(Knot<TPos, TValue> k1, Knot<TPos, TValue> k2) {
        x_k = k1.getPosition();
        x_k1 = k2.getPosition();
        y_k = k1.getValue();
        y_k1 = k2.getValue();
        m_k = k1.getSlope();
        m_k1 = k2.getSlope();
    }

    Segment(const Segment &other)
        : x_k(other.x_k), x_k1(other.x_k1), y_k(other.y_k), y_k1(other.y_k1), m_k(other.m_k),
          m_k1(other.m_k1) {
    }

    Segment &operator=(const Segment &other) {
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

    TValue getValue(TPos position) const {
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
    AnchoredCurve() {
        _knots.clear();
        _segments.clear();
    }

    ~AnchoredCurve() = default;

    AnchoredCurve(const AnchoredCurve &other) {
        _knots = other._knots;
        _segments = other._segments;
    }

    AnchoredCurve &operator=(const AnchoredCurve &other) {
        if (this != &other) {
            _knots = other._knots;
            _segments = other._segments;
        }
        return *this;
    }

    explicit AnchoredCurve(const QList<Knot<TPos, TValue>> &knots) {
        _knots = knots;
        updateAll();
    }

    AnchoredCurve(std::initializer_list<TPos> positions, std::initializer_list<TValue> values) {
        // 使用 std::initializer_list 的 size() 成员函数
        for (size_t i = 0; i < positions.size(); ++i) {
            _knots.push_back(Knot<TPos, TValue>(*(positions.begin() + i), *(values.begin() + i)));
        }
        updateAll();
    }

    TValue getValue(TPos position) const {
        if (_segments.count() <= 0) {
            return TValue();
        }
        auto it = std::lower_bound(_knots.begin(), _knots.end(),
                                   Knot<TPos, TValue>(position, TValue()), knotCmp<TPos, TValue>);
        if (it == _knots.end()) {
            return _knots.back().getValue();
        }
        if (it == _knots.begin()) {
            return _knots.front().getValue();
        }
        auto index = std::distance(_knots.begin(), it);
        return _segments[index - 1].getValue(position);
    }

    std::vector<std::pair<TPos, TValue>> getValueLinspace(TPos start, TPos end, int num) const {
        std::vector<std::pair<TPos, TValue>> res;
        if (num == 0) {
            return res;
        }
        auto knot_it = std::lower_bound(_knots.begin(), _knots.end(), Knot<TPos, TValue>(start, 0),
                                        knotCmp<TPos, TValue>);
        int curr_segment_index = std::distance(_knots.begin(), knot_it) - 1;
        double num_d = num;
        for (int i = 0; i < num; i++) {
            double w = (static_cast<double>(i) / num_d);
            TPos position = static_cast<TPos>((1.0 - w) * start + w * end);

            if (curr_segment_index < 0) {
                if (_knots.front().getPosition() <= position) {
                    curr_segment_index = 0;
                }
            } else if (curr_segment_index >= _segments.count()) {

            } else {
                if (_knots[curr_segment_index + 1].getPosition() <= position) {
                    ++curr_segment_index;
                }
            }

            if (curr_segment_index < 0) {
                res.push_back(std::make_pair(position, _knots.front().getValue()));
            } else if (curr_segment_index >= _segments.count()) {
                res.push_back(std::make_pair(position, _knots.back().getValue()));
            } else {
                res.push_back(
                    std::make_pair(position, _segments[curr_segment_index].getValue(position)));
            }
        }
        return res;
    }

    void insert(Knot<TPos, TValue> knot) {
        // 边界条件，knots.count()==0
        if (_knots.count() <= 0) {
            _knots.push_back(knot);
            return;
        }

        auto it = std::lower_bound(_knots.begin(), _knots.end(), knot, knotCmp<TPos, TValue>);
        int index;
        if (it != _knots.end()) {
            index = std::distance(_knots.begin(), it);
        } else {
            index = _knots.count();
        }

        if (index >= _knots.count()) {
            _knots.push_back(knot);
            _segments.push_back(Segment<TPos, TValue>());
        } else {
            if (!(_knots[index] == knot)) {
                _knots.insert(index, knot);
                if (_segments.count() <= 0) {
                    _segments.push_back(Segment<TPos, TValue>());
                } else {
                    _segments.insert(index, Segment<TPos, TValue>());
                }
            } else {
                _knots[index] = knot;
            }
        }
        update(index - 1, index + 1);
    }

    void insert(TPos position, TValue value) {
        insert(Knot<TPos, TValue>(position, value));
    }

    void merge(const QList<Knot<TPos, TValue>> &knots) {
        _knots = _knots + knots;
        updateAll();
    }

    void merge(const AnchoredCurve &other) {
        merge(other.getKnots());
    }

    void merge(std::initializer_list<TPos> positions, std::initializer_list<TValue> values) {
        QList<Knot<TPos, TValue>> knots_new;
        for (size_t i = 0; i < positions.size(); ++i) {
            knots_new.push_back(
                Knot<TPos, TValue>(*(positions.begin() + i), *(values.begin() + i)));
        }
        merge(knots_new);
    }

    void remove(TPos x) {
        // 边界条件，knots.count()==0
        if (_knots.count() <= 0) {
            return;
        }

        auto it = std::lower_bound(_knots.begin(), _knots.end(), Knot<TPos, TValue>(x, TValue()),
                                   knotCmp<TPos, TValue>);
        if (it == _knots.end()) {
            return;
        } else if (it->getPosition() == x) {
            int index = std::distance(_knots.begin(), it);
            _knots.removeAt(index);
            if (index < _segments.count()) {
                _segments.removeAt(index);
            } else {
                if (!_segments.isEmpty())
                    _segments.removeLast();
            }
            update(index - 1, index);
        }
    }

    void removeRange(TPos l, TPos r) { // 移除闭区间[l,r]
        auto start_it = std::lower_bound(_knots.begin(), _knots.end(),
                                         Knot<TPos, TValue>(l, TValue()), knotCmp<TPos, TValue>);
        auto end_it = std::upper_bound(_knots.begin(), _knots.end(),
                                       Knot<TPos, TValue>(r, TValue()), knotCmp<TPos, TValue>);
        int start_index = std::distance(_knots.begin(), start_it);
        int end_index = std::distance(_knots.begin(), end_it);
        _knots.erase(start_it, end_it);
        _segments.erase(_segments.begin() + start_index, _segments.begin() + end_index);
        update(start_index - 1, start_index);
    }

    const QList<Knot<TPos, TValue>> &getKnots() const {
        return _knots;
    }

    const QList<Segment<TPos, TValue>> &getSegments() const {
        return _segments;
    }

private:
    QList<Knot<TPos, TValue>> _knots;
    QList<Segment<TPos, TValue>> _segments;

    [[nodiscard]] double getInteriorSlope(int index) const {
        TValue delta_y_l = (_knots[index].getValue() - _knots[index - 1].getValue());
        TValue delta_y_r = (_knots[index + 1].getValue() - _knots[index].getValue());
        if ((delta_y_l * delta_y_r) <= 0) {
            return 0;
        }
        TPos delta_x_l = (_knots[index].getPosition() - _knots[index - 1].getPosition());
        TPos delta_x_r = (_knots[index + 1].getPosition() - _knots[index].getPosition());
        double slope_l = delta_y_l / delta_x_l;
        double slope_r = delta_y_r / delta_x_r;
        double w_l = delta_x_l + 2 * delta_x_r, w_r = 2 * delta_x_l + delta_x_r;
        return (w_l + w_r) / (w_l / slope_l + w_r / slope_r);
    }

    void updateAll() {
        if (_knots.count() <= 1) {
            return;
        }
        updateAllKnots();
        updateAllSegments();
    }

    void updateAllKnots() {
        int len = _knots.count();

        std::sort(_knots.begin(), _knots.end(), knotCmp<TPos, TValue>);
        double slope_start = static_cast<double>(_knots[1].getValue() - _knots.front().getValue()) /
                             (_knots[1].getPosition() - _knots.front().getPosition());
        _knots.front().setSlope(slope_start);
        double slope_end =
            static_cast<double>(_knots.back().getValue() - _knots[len - 2].getValue()) /
            (_knots.back().getPosition() - _knots[len - 2].getPosition());
        _knots.back().setSlope(slope_end);
        if (len <= 2) {
            return;
        }

        for (int i = 1; i < len - 1; i++) {
            _knots[i].setSlope(getInteriorSlope(i));
        }
    }

    void updateAllSegments() {
        _segments.clear();
        for (int i = 0; i < _knots.count() - 1; i++) {
            _segments.push_back(Segment<TPos, TValue>(_knots.at(i), _knots.at(i + 1)));
        }
    }

    void update(int start_index, int end_index) { // 更新闭区间[start_index,end_index]
        if (_knots.count() <= 1) {
            return;
        }
        updateKnots(start_index, end_index);
        updateSegments(start_index, end_index);
    }

    void updateKnots(int start_index, int end_index) {
        int len = _knots.count();

        for (int i = start_index; i <= end_index; i++) {
            if (i < 0 || i >= len) {
                continue;
            }
            if (i == 0) {
                _knots[i].setSlope((_knots[1].getValue() - _knots.front().getValue()) /
                                   (_knots[1].getPosition() - _knots.front().getPosition()));
            } else if (i == len - 1) {
                _knots[i].setSlope((_knots.back().getValue() - _knots[len - 2].getValue()) /
                                   (_knots.back().getPosition() - _knots[len - 2].getPosition()));
            } else {
                _knots[i].setSlope(getInteriorSlope(i));
            }
        }
    }

    void updateSegments(int start_index, int end_index) {
        for (int i = start_index - 1; i < end_index + 1; i++) {
            if (i < 0 || i >= _segments.count()) {
                continue;
            }
            _segments[i] = Segment<TPos, TValue>(_knots[i], _knots[i + 1]);
        }
    }
};

#endif // ANCHOREDCURVE_H
