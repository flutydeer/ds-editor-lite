//
// Created by fluty on 2024/2/6.
//

#ifndef MATHUTILS_H
#define MATHUTILS_H



class MathUtils {
public:
    static int round(int tick, int step) {
        int times = tick / step;
        int mod = tick % step;
        if (mod > step / 2)
            return step * (times + 1);
        return step * times;
    }

    static int roundDown(int tick, int step) {
        int times = tick / step;
        return step * times;
    }
};



#endif // MATHUTILS_H
