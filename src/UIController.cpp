#include <UIController.h>

UIController::UIController(hd44780_I2Cexp& lcd, // NOLINT(*-pro-type-member-init
                           Button& buttonA,
                           Button& buttonB,
                           Button& buttonC,
                           Button& rotaryButton,
                           RotaryEncoder& rotaryEncoder,
                           OutputChannel* outputChannels[3]) :
    lcd_(lcd),
    buttonA_(buttonA),
    buttonB_(buttonB),
    buttonC_(buttonC),
    rotaryButton_(rotaryButton),
    rotaryEncoder_(rotaryEncoder) {
    for (int i = 0; i < 3; i++) {
        outputChannels_[i] = outputChannels[i];
    }
}

void UIController::processInputs() {
    rotaryButton_.read();
    buttonA_.read();
    buttonB_.read();
    buttonC_.read();

    const long newEncoderPosition = rotaryEncoder_.getPosition();
    if (encoderPosition != newEncoderPosition) {
        const long diff = newEncoderPosition - encoderPosition;
        if (screen_ == MAIN) {
            outputChannelIndex_ = static_cast<int>(outputChannelIndex_ + diff % MAX_OUTPUT_CHANNELS +
                    MAX_OUTPUT_CHANNELS) %
                MAX_OUTPUT_CHANNELS;
            updateScreen();
        } else {
            OutputChannel* outputChannel = getOutputChannel();
            const int64_t newFreq        = static_cast<int64_t>(outputChannel->getSetFrequency())
                + diff * FREQUENCY_ADJUSTMENTS[frequencyAdjustmentIndex_].delta;
            outputChannel->setFrequency(
                static_cast<uint64_t>(max(FREQUENCY_MIN, min(FREQUENCY_MAX, newFreq)))
            );
            updateScreen();
        }
    }
    encoderPosition = newEncoderPosition;

    if (screen_ == MAIN) {
        if (buttonA_.isPressed()) {
            getOutputChannel()->turnOn();
            updateScreen();
        }

        if (buttonB_.isPressed()) {
            getOutputChannel()->turnOff();
            updateScreen();
        }

        if (buttonC_.isPressed() || rotaryButton_.isPressed()) {
            screen_ = OUTPUT_CHANNEL;
            updateScreen();
        }
    } else {
        if (buttonA_.isPressed() || buttonB_.isPressed() || buttonC_.isPressed()) {
            screen_ = MAIN;
            updateScreen();
        }

        if (rotaryButton_.isPressed()) {
            frequencyAdjustmentIndex_ = (frequencyAdjustmentIndex_ + 1) % NUMBER_OF_FREQUENCY_ADJUSTMENTS;
            updateScreen();
        }
    }
}

void UIController::updateScreen() const {
    lcd_.noCursor();
    lcd_.noBlink();
    lcd_.clear();
    switch (screen_) {
        case MAIN:
            printMainScreen();
            break;
        case OUTPUT_CHANNEL:
            printOutputChannelScreen();
            break;
        default:
            printMainScreen();
            break;
    }
}

void UIController::printMainScreen() const {
    char frequencyBuffer[15];
    lcd_.setCursor(0, 0);
    if (outputChannelIndex_ == 0) {
        lcd_.print(F(">CH0: "));
    } else {
        lcd_.print(F(" CH0: "));
    }
    getOutputChannelFrequency(outputChannels_[0], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(0, 1);
    if (outputChannelIndex_ == 1) {
        lcd_.print(F(">CH1: "));
    } else {
        lcd_.print(F(" CH1: "));
    }
    getOutputChannelFrequency(outputChannels_[1], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(0, 2);
    if (outputChannelIndex_ == 2) {
        lcd_.print(F(">CH2: "));
    } else {
        lcd_.print(F(" CH2: "));
    }
    getOutputChannelFrequency(outputChannels_[2], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(0, 3);
    lcd_.print(F(" A:On B:Off C:Config"));

    lcd_.setCursor(0, outputChannelIndex_);
    lcd_.blink();
}

void UIController::printOutputChannelScreen() const {
    char frequencyBuffer[14];
    lcd_.setCursor(0, 0);
    lcd_.print(F("Channel "));
    lcd_.print(outputChannelIndex_);

    lcd_.setCursor(0, 1);
    lcd_.print(F("Set:   "));
    getOutputChannelFrequencyPadded(outputChannels_[outputChannelIndex_]->getSetFrequency(), frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(0, 2);
    lcd_.print(F("Real:  "));
    getOutputChannelFrequencyPadded(outputChannels_[outputChannelIndex_]->getActualFrequency(), frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(0, 3);
    lcd_.print(F("A|B|C: Back"));

    lcd_.setCursor(FREQUENCY_ADJUSTMENTS[frequencyAdjustmentIndex_].col + 7, 1);
    lcd_.cursor();
}

void UIController::getOutputChannelFrequency(const OutputChannel* outputChannel, char* out) {
    // Input:  centi-Hz  e.g. 123456789 = 1,234,567.89 Hz
    // Output: right-aligned in 14 chars, e.g. "  10.000,00 Hz"

    const uint64_t cHz     = outputChannel->getActualFrequency();
    const uint64_t hz      = cHz / 100;
    const uint64_t decimal = cHz % 100;

    const uint64_t g0 = hz % 1000;
    const uint64_t g1 = (hz / 1000) % 1000;
    const uint64_t g2 = hz / 1000000;

    char tmp[15];
    if (g2 > 0) {
        sprintf(tmp, "%lu.%03lu.%03lu,%02lu", g2, g1, g0, decimal);
    } else if (g1 > 0) {
        sprintf(tmp, "%lu.%03lu,%02lu", g1, g0, decimal);
    } else {
        sprintf(tmp, "%lu,%02lu", g0, decimal);
    }

    // Right-align in 14 chars
    const int len     = static_cast<int>(strlen(tmp));
    const int padding = 14 - len;
    for (int i = 0; i < padding; i++) out[i] = ' ';
    strcpy(out + padding, tmp);
}

void UIController::getOutputChannelFrequencyPadded(const uint64_t frequency, char* out) {
    // Examples:
    //   Input centi-Hz:           0  →  "00.000.000,00"
    //   Input centi-Hz:       12345  →  "00.000.123,45"
    //   Input centi-Hz:   123456789  →  "00.123.456,89"
    //   Input centi-Hz: 12345678900  →  "12.345.678,00"

    const uint64_t hz      = frequency / 100UL;
    const uint64_t decimal = frequency % 100UL;

    const uint64_t g0 = hz % 1000UL; // Hz  group (0–999)
    const uint64_t g1 = (hz / 1000UL) % 1000UL; // kHz group (0–999)
    const uint64_t g2 = (hz / 1000000UL) % 100UL; // MHz group (0–99)

    sprintf(out, "%02lu.%03lu.%03lu,%02lu", g2, g1, g0, decimal);
    // Result is always exactly 13 characters + NUL terminator.
    // Buffer must be at least 14 bytes.
}

OutputChannel* UIController::getOutputChannel() const {
    return outputChannels_[outputChannelIndex_];
}
