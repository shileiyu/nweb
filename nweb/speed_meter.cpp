#include <memory.h>
#include <float.h>
#include "nweb.h"
#include "speed_meter.h"

namespace nweb
{

static uint32_t TickCount()
{
    return GetTickCount() / 1000;
}

SpeedMeter::SpeedMeter()
{
    Reset();
}

void SpeedMeter::Reset()
{
    memset(records_, 0, sizeof(records_));
    for(uint8_t i  = 0; i < kMaxRecord; ++i)
    {
        uint8_t next = (i + 1) % kMaxRecord;
        records_[i].next = &records_[next];
    }
    first_ = &records_[kMaxRecord - 1];
    last_ = &records_[0];
    hit_count_ = 0;
}

void SpeedMeter::Note(uint64_t value)
{
    Roll();
    first_->value = value;
}

void SpeedMeter::Accum(uint64_t value)
{
    Roll();
    first_->value += value;
}

float SpeedMeter::Speed() const
{
    if(!first_->tick || !last_->tick)
        return -1.0f;
    float delta = static_cast<float>(first_->value - last_->value);
    float elapse = static_cast<float>(first_->tick - last_->tick);
    return elapse >= 1.0f ? delta / elapse : -1.0f;
}

void SpeedMeter::Roll()
{
    uint32_t now = TickCount();
    if(first_->tick != now || hit_count_ >= kMaxHitCount)
    {
        first_->next->value = first_->value;
        first_->next->tick = now;
        first_ = first_->next;
        last_ = last_->next;
        hit_count_ = 0;
    }
    ++hit_count_;
}

}