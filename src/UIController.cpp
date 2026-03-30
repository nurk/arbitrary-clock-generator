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

void UIController::update(const boolean force) {
    processInputs();

    if (force || millis() > lastUpdateMillis_ + 215) {
        lcd_.clear();
        switch (screen_) {
            case MAIN:
                printMainScreen();
                break;
            case OUTPUT_CHANNEL:
                // todo implement output channel config screen
                break;
            default:
                printMainScreen();
                break;
        }
        lastUpdateMillis_ = millis();
    }
}

void UIController::printMainScreen() const {
    char frequencyBuffer[21];
    lcd_.setCursor(0, 0);
    if (outputChannelIndex_ == 0) {
        lcd_.print(F(">CH0: "));
    } else {
        lcd_.print(F(" CH0: "));
    }
    getOutputChannelFrequency(outputChannels_[0], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(1, 0);
    if (outputChannelIndex_ == 1) {
        lcd_.print(F(">CH1: "));
    } else {
        lcd_.print(F(" CH1: "));
    }
    getOutputChannelFrequency(outputChannels_[1], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(2, 0);
    if (outputChannelIndex_ == 2) {
        lcd_.print(F(">CH2: "));
    } else {
        lcd_.print(F(" CH2: "));
    }
    getOutputChannelFrequency(outputChannels_[2], frequencyBuffer);
    lcd_.print(frequencyBuffer);

    lcd_.setCursor(3, 0);
    lcd_.print(F(" A:On B:Off C:Config"));
}

void UIController::getOutputChannelFrequency(const OutputChannel* outputChannel, char* out) {
    // Input:  centi-Hz  e.g. 123456789 = 1,234,567.89 Hz
    // Output: right-aligned in 14 chars, e.g. "  10.000,00 Hz"

    const uint32_t cHz     = outputChannel->getFrequency();
    const uint32_t hz      = cHz / 100;
    const uint32_t decimal = cHz % 100;

    const uint32_t g0 = hz % 1000;
    const uint32_t g1 = (hz / 1000) % 1000;
    const uint32_t g2 = hz / 1000000;

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

void UIController::processInputs() {
    rotaryButton_.read();
    buttonA_.read();
    buttonB_.read();
    buttonC_.read();

    const int newEncoderPosition = rotaryEncoder_.getPosition(); // NOLINT(*-narrowing-conversions)
    if (encoderPosition != newEncoderPosition) {
        const int diff = newEncoderPosition - encoderPosition;
        if (screen_ == MAIN) {
            outputChannelIndex_ = (outputChannelIndex_ + diff + MAX_OUTPUT_CHANNELS) % MAX_OUTPUT_CHANNELS;
            update(true);
        } else {
            OutputChannel* outputChannel = getOutputChannel();
            outputChannel->setFrequency(max(FREQUENCY_MIN,
                                            min(FREQUENCY_MAX,
                                                outputChannel->getFrequency() + (diff * FREQUENCY_ADJUSTMENTS[
                                                    frequencyAdjustmentIndex_].delta))));
        }
    }
    encoderPosition = newEncoderPosition;

    if (screen_ == MAIN) {
        if (buttonA_.isPressed()) {
            getOutputChannel()->turnOn();
            update(true);
        }

        if (buttonB_.isPressed()) {
            getOutputChannel()->turnOff();
            update(true);
        }

        if (buttonC_.isPressed() || rotaryButton_.isPressed()) {
            screen_ = OUTPUT_CHANNEL;
            update(true);
        }
    }
}

OutputChannel* UIController::getOutputChannel() const {
    return outputChannels_[outputChannelIndex_];
}
