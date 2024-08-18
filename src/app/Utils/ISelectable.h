//
// Created by fluty on 24-2-15.
//

#ifndef ISELECTABLE_H
#define ISELECTABLE_H

class ISelectable {
public:
    [[nodiscard]] bool selected() const {
        return m_selected;
    }

    void setSelected(bool b) {
        m_selected = b;
    }

protected:
    bool m_selected = false;
};

#endif // ISELECTABLE_H
