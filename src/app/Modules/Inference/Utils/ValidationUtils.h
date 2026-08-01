#ifndef VALIDATIONUTILS_H
#define VALIDATIONUTILS_H


class SingingClip;

class ValidationUtils {
public:
    static bool canInferDuration(const SingingClip &clip);
};

#endif //VALIDATIONUTILS_H
