#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdint.h>

enum class EasingType {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseOutBounce,
    EaseOutElastic
};

class Easing {
public:
    static float Apply(float t, EasingType type);
};

class Animation {
public:
    Animation(float startVal, float endVal, uint32_t durationMs, EasingType type = EasingType::Linear);
    
    void Start(uint32_t currentTime);
    void Update(uint32_t currentTime);
    float GetValue() const;
    bool IsFinished() const;
    
    void Reset();

private:
    float startValue;
    float endValue;
    float currentValue;
    uint32_t startTime;
    uint32_t duration;
    EasingType easingType;
    bool finished;
    bool started;
};

#endif
