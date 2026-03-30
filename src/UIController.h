#ifndef ARBITRARY_CLOCK_GENERATOR_UICONTROLLER_H
#define ARBITRARY_CLOCK_GENERATOR_UICONTROLLER_H

// ReSharper disable CppUnusedIncludeDirective
#include <OutputChannel.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <JC_Button.h>
#include <RotaryEncoder.h>


class UIController {
public:
    UIController(hd44780_I2Cexp& lcd,
                 Button& buttonA,
                 Button& buttonB,
                 Button& buttonC,
                 Button& rotaryButton,
                 RotaryEncoder& rotaryEncoder,
                 OutputChannel* outputChannels[3]);
    void update(boolean force);

private:
    void processInputs();
    OutputChannel* getOutputChannel() const;
    void printMainScreen() const;
    static void getOutputChannelFrequency(const OutputChannel* outputChannel, char* out);

    hd44780_I2Cexp& lcd_;
    Button& buttonA_;
    Button& buttonB_;
    Button& buttonC_;
    Button& rotaryButton_;
    RotaryEncoder& rotaryEncoder_;
    OutputChannel* outputChannels_[3];

    enum Screen {
        MAIN,
        OUTPUT_CHANNEL
    };

    Screen screen_                  = MAIN;
    unsigned long lastUpdateMillis_ = 0;
    int encoderPosition             = 0;
    int outputChannelIndex_         = 0;
    const int MAX_OUTPUT_CHANNELS   = 3;

    const uint32_t FREQUENCY_MAX = 10000000000;
    const uint32_t FREQUENCY_MIN = 0;

    struct FrequencyAdjustment {
        uint8_t col;
        uint32_t delta;
    };

    const FrequencyAdjustment FREQUENCY_ADJUSTMENTS[10] = {
        {0, 1000000000},
        {0, 100000000},
        {1, 10000000},
        {3, 1000000},
        {4, 100000},
        {5, 10000},
        {7, 1000},
        {8, 100},
        {9, 10},
        {11, 1}
    };
    int8_t frequencyAdjustmentIndex_              = 0;
    const uint8_t NUMBER_OF_FREQUENCY_ADJUSTMENTS = 9;
};

#endif //ARBITRARY_CLOCK_GENERATOR_UICONTROLLER_H
