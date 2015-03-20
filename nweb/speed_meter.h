#ifndef NWEB_SPEED_RECORDER_H_
#define NWEB_SPEED_RECORDER_H_

#include <stdint.h>

namespace nweb
{

class SpeedMeter
{
    struct Record
    {
        Record * next;
        uint64_t value;
        uint32_t tick;
    };
    static const uint32_t kMaxRecord = 8;
    static const uint32_t kMaxHitCount = 200;
public:
    SpeedMeter();

    void Reset();

    void Note(uint64_t value);

    void Accum(uint64_t value);

    float Speed() const;
private:
    void Roll();

private:
    Record records_[kMaxRecord];
    Record * first_;
    Record * last_;
    uint32_t hit_count_;
};


}
#endif