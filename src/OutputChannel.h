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

    void turnOff();
    void turnOn();
    uint32_t setFrequency(uint32_t frequencyCentiHz);
    uint32_t getActualFrequency() const;
    uint32_t getSetFrequency() const;

private:
    const uint8_t selectPin;
    const uint8_t ledPin;
    TCB_t& tcb;
    Si5351& si5351;
    const si5351_clock siClock;
    uint32_t actualFrequencyCentiHz_    = 0;
    uint32_t setFrequencyCentiHz_       = 0;
    const uint32_t SWITCHOVER_FREQUENCY = 400000UL;;
    boolean isOn_                       = false;

    uint32_t setTCBFrequency(uint32_t frequencyCentiHz) const;
    uint32_t setSiFrequency(uint32_t frequencyCentiHz) const;
};


#endif //ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
