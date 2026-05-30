#ifndef ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
#define ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H

#include <si5351.h>


class OutputChannel {
public:
    OutputChannel(uint8_t selectPin,
                  uint8_t ledPin,
                  TCB_t* tcb,
                  Si5351& si5351,
                  si5351_clock siClock);

    void turnOff();
    void turnOn();
    void toggle();
    uint64_t setFrequency(uint64_t frequencyCentiHz);
    uint64_t getActualFrequency() const;
    uint64_t getSetFrequency() const;
    void saveToEEPROM() const;
    void loadFromEEPROM();

private:
    const uint8_t selectPin;
    const uint8_t ledPin;
    TCB_t* tcb;
    Si5351& si5351;
    const si5351_clock siClock;
    uint64_t actualFrequencyCentiHz    = 0;
    uint64_t setFrequencyCentiHz       = 0;
    const uint64_t SWITCHOVER_FREQUENCY = 400000UL;
    boolean isOn                       = false;

    uint64_t setTCBFrequency(uint64_t frequencyCentiHz) const;
    uint64_t setSiFrequency(uint64_t frequencyCentiHz) const;
    int getEEPROMOffset() const;
};


#endif //ARBITRARY_CLOCK_GENERATOR_OUTPUTCHANNEL_H
