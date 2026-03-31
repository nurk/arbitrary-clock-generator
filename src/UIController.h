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
    void processInputs();
    void updateScreen() const;

private:
    OutputChannel* getOutputChannel() const;
    void printMainScreen() const;
    void printOutputChannelScreen() const;
    static void getOutputChannelFrequency(const OutputChannel* outputChannel, char* out);
    static void getOutputChannelFrequencyPadded(const uint32_t frequency, char* out);

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

    Screen screen_                = MAIN;
    int encoderPosition           = 0;
    int outputChannelIndex_       = 0;
    const int MAX_OUTPUT_CHANNELS = 3;

    const uint32_t FREQUENCY_MAX = 9999999999;
    const uint32_t FREQUENCY_MIN = 0;

    struct FrequencyAdjustment {
        int col;
        uint32_t delta;
    };

    const FrequencyAdjustment FREQUENCY_ADJUSTMENTS[10] = {
        {0, 1000000000},
        {1, 100000000},
        {3, 10000000},
        {4, 1000000},
        {5, 100000},
        {7, 10000},
        {8, 1000},
        {9, 100},
        {11, 10},
        {12, 1}
    };
    int frequencyAdjustmentIndex_             = 0;
    const int NUMBER_OF_FREQUENCY_ADJUSTMENTS = 10;
};

#endif //ARBITRARY_CLOCK_GENERATOR_UICONTROLLER_H
