#ifndef ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
#define ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
#include "si5351.h"


class OutputChannel {
public:
    OutputChannel(uint8_t selectPin,
                  uint8_t ledPin,
                  TCB_t& tcb,
                  Si5351& si5351,
                  si5351_clock siClock);

    void turnOff() const;
    uint32_t setFrequency(uint32_t frequencyCentiHz) const;

private:
    const uint8_t selectPin;
    const uint8_t ledPin;
    Si5351& si5351;
    const si5351_clock siClock;
    TCB_t& tcb;

    uint32_t setTCBFrequency(uint32_t frequencyCentiHz) const;
    uint32_t setSiFrequency(uint32_t frequencyCentiHz) const;
};


#endif //ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
