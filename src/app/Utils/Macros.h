//
// Created by fluty on 24-8-10.
//

#ifndef MACROS_H
#define MACROS_H

#define PROPERTY(ClassName, FieldName, InitValue)                                                  \
public:                                                                                            \
    [[nodiscard]] ClassName FieldName() const {                                                    \
        return m_##FieldName;                                                                      \
    }                                                                                              \
    void set_##FieldName(const ClassName &FieldName) {                                             \
        m_##FieldName = FieldName;                                                                 \
    }                                                                                              \
                                                                                                   \
private:                                                                                           \
    ClassName m_##FieldName = InitValue;


#endif // MACROS_H
