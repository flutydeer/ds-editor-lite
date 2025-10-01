//
// Created by fluty on 24-8-10.
//

#ifndef MACROS_H
#define MACROS_H

#define PROPERTY(ClassName, FieldName, InitValue)                                                  \
public:                                                                                            \
    ClassName FieldName() const {                                                                  \
        return m_##FieldName;                                                                      \
    }                                                                                              \
    void set_##FieldName(const ClassName &FieldName) {                                             \
        m_##FieldName = FieldName;                                                                 \
    }                                                                                              \
                                                                                                   \
private:                                                                                           \
    ClassName m_##FieldName = InitValue;

// Interface
#define LITE_INTERFACE class

#define I_DECL(InterfaceName)                                                                      \
public:                                                                                            \
    virtual ~InterfaceName() = default;

#define I_METHOD(Method) virtual Method = 0

#define I_NODSCD(Method) virtual Method = 0

#define I_MEMBER(Member)                                                                           \
protected:                                                                                         \
    Member


// #define DECL_INTERFACE(InterfaceName)                                                              \
//     class InterfaceName {                                                                          \
//     public:                                                                                        \
//         virtual ~##InterfaceName() = default;
// #define END_INTERFACE };

#endif // MACROS_H
