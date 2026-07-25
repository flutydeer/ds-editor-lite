#include "IdGenerator.h"

IdGenerator *IdGenerator::instance() {
    static IdGenerator obj;
    return &obj;
}
