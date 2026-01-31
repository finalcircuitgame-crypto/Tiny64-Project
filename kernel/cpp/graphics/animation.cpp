#include "animation.h"
#include "../../c/lib/math.h"

#define PI 3.1415926535f

// Helper for pow(2, x)
static float pow2_f(float exp) {
    if (exp == 0) return 1.0f;
    if (exp > 0) {
        float res = 1.0f;
        while (exp >= 1.0f) { res *= 2.0f; exp -= 1.0f; }
        if (exp > 0) res *= (1.0f + exp * 0.693147f); // Linear approximation for fractional part
        return res;
    } else {
        return 1.0f / pow2_f(-exp);
    }
}

float Easing::Apply(float t, EasingType type) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (type) {
        case EasingType::Linear: return t;
        case EasingType::EaseInQuad: return t * t;
        case EasingType::EaseOutQuad: return t * (2 - t);
        case EasingType::EaseInOutQuad: return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case EasingType::EaseOutBounce: {
            if (t < (1 / 2.75f)) {
                return 7.5625f * t * t;
            } else if (t < (2 / 2.75f)) {
                t -= (1.5f / 2.75f);
                return 7.5625f * t * t + 0.75f;
            } else if (t < (2.5f / 2.75f)) {
                t -= (2.25f / 2.75f);
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= (2.625f / 2.75f);
                return 7.5625f * t * t + 0.984375f;
            }
        }
        case EasingType::EaseOutElastic: {
            if (t == 0) return 0;
            if (t == 1) return 1;
            float p = 0.3f;
            return (float)pow2_f(-10 * t) * (float)sin((t * 10 - 0.75f) * ((2 * PI) / p)) + 1;
        }
        default: return t;
    }
}

Animation::Animation(float start, float end, uint32_t dur, EasingType type)
    : startValue(start), endValue(end), duration(dur), easingType(type), 
      finished(false), started(false), currentValue(start) {}

void Animation::Start(uint32_t currentTime) {
    startTime = currentTime;
    started = true;
    finished = false;
    currentValue = startValue;
}

void Animation::Update(uint32_t currentTime) {
    if (!started) return;
    if (finished) return;

    if (currentTime < startTime) { // Timer overflow handling? or just sanity
        currentValue = startValue;
        return;
    }

    uint32_t elapsed = currentTime - startTime;
    if (elapsed >= duration) {
        currentValue = endValue;
        finished = true;
    } else {
        float t = (float)elapsed / (float)duration;
        float easedT = Easing::Apply(t, easingType);
        currentValue = startValue + (endValue - startValue) * easedT;
    }
}

float Animation::GetValue() const {
    return currentValue;
}

bool Animation::IsFinished() const {
    return finished;
}

void Animation::Reset() {
    started = false;
    finished = false;
    currentValue = startValue;
}
