#ifndef __PID_H
#define __PID_H

class PID {
    float Kp, Ki, Kd;
    float target = 0;
    float actual = 0;
    float e = 0, e_last = 0, e_prev = 0;
    float out = 0;
    float out_min, out_max;
public:
    PID() : Kp(0), Ki(0), Kd(0), out_min(0), out_max(0) {}

    void init(float kp, float ki, float kd, float min, float max) {
        Kp = kp; Ki = ki; Kd = kd;
        out_min = min; out_max = max;
        reset();
    }

    float compute() {
        e = target - actual;
        float delta = Kp * (e - e_last)
                    + Ki * e
                    + Kd * (e - 2.0f * e_last + e_prev);
        out += delta;

        if (out > out_max) out = out_max;
        if (out < out_min) out = out_min;

        e_prev = e_last;
        e_last = e;
        return out;
    }

    void reset() {
        e = e_last = e_prev = 0;
        out = 0;
    }

    void setTarget(float v) { target = v; }
    void setActual(float v) { actual = v; }
    float getTarget() const { return target; }
    float getActual() const { return actual; }
    float getOutput() const { return out; }
};

#endif
