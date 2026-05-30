#include <UIController.h>

UIController::UIController(hd44780_I2Cexp& lcd, // NOLINT(*-pro-type-member-init
                           Button& buttonA,
                           Button& buttonB,
                           Button& buttonC,
                           Button& rotaryButton,
                           RotaryEncoder& rotaryEncoder,
                           OutputChannel* outputChannels[3]) :
    lcd(lcd),
    buttonA(buttonA),
    buttonB(buttonB),
    buttonC(buttonC),
    rotaryButton(rotaryButton),
    rotaryEncoder(rotaryEncoder) {
    for (int i = 0; i < 3; i++) {
        this->outputChannels[i] = outputChannels[i];
    }
}

void UIController::processInputs() {
    rotaryButton.read();
    buttonA.read();
    buttonB.read();
    buttonC.read();

    // test if this tick needs to stay
    rotaryEncoder.tick();
    const long newEncoderPosition = rotaryEncoder.getPosition();
    if (encoderPosition != newEncoderPosition) {
        const long diff = newEncoderPosition - encoderPosition;
        if (screen == MAIN) {
            outputChannelIndex = static_cast<int>(outputChannelIndex + diff % MAX_OUTPUT_CHANNELS +
                    MAX_OUTPUT_CHANNELS) %
                MAX_OUTPUT_CHANNELS;
            updateScreen();
        } else {
            OutputChannel* outputChannel = getOutputChannel();
            const int64_t newFreq        = static_cast<int64_t>(outputChannel->getSetFrequency())
                + diff * FREQUENCY_ADJUSTMENTS[frequencyAdjustmentIndex].delta;
            outputChannel->setFrequency(
                static_cast<uint64_t>(max(FREQUENCY_MIN, min(FREQUENCY_MAX, newFreq)))
            );
            updateScreen();
        }
    }
    encoderPosition = newEncoderPosition;

    if (screen == MAIN) {
        if (buttonA.wasPressed()) {
            getOutputChannel()->toggle();
            updateScreen();
        }

        if (buttonB.wasPressed()) {
            for (int i = 0; i < MAX_OUTPUT_CHANNELS; i++) {
                outputChannels[i]->toggle();
            }
            updateScreen();
        }

        if (buttonC.wasPressed() || rotaryButton.wasPressed()) {
            screen = OUTPUT_CHANNEL;
            updateScreen();
        }
    } else {
        if (buttonA.wasPressed() || buttonB.wasPressed() || buttonC.wasPressed()) {
            screen = MAIN;
            getOutputChannel()->saveToEEPROM();
            updateScreen();
        }

        if (rotaryButton.wasPressed()) {
            frequencyAdjustmentIndex = (frequencyAdjustmentIndex + 1) % NUMBER_OF_FREQUENCY_ADJUSTMENTS;
            updateScreen();
        }
    }
}

void UIController::updateScreen() const {
    lcd.noCursor();
    lcd.noBlink();
    switch (screen) {
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
    lcd.setCursor(0, 0);
    if (outputChannelIndex == 0) {
        lcd.print(F(">CH0: "));
    } else {
        lcd.print(F(" CH0: "));
    }
    getOutputChannelFrequency(outputChannels[0], frequencyBuffer);
    lcd.print(frequencyBuffer);

    lcd.setCursor(0, 1);
    if (outputChannelIndex == 1) {
        lcd.print(F(">CH1: "));
    } else {
        lcd.print(F(" CH1: "));
    }
    getOutputChannelFrequency(outputChannels[1], frequencyBuffer);
    lcd.print(frequencyBuffer);

    lcd.setCursor(0, 2);
    if (outputChannelIndex == 2) {
        lcd.print(F(">CH2: "));
    } else {
        lcd.print(F(" CH2: "));
    }
    getOutputChannelFrequency(outputChannels[2], frequencyBuffer);
    lcd.print(frequencyBuffer);

    lcd.setCursor(0, 3);
    lcd.print(F("A:Tgl B:TglAll C:Cfg"));

    lcd.setCursor(0, outputChannelIndex);
    lcd.blink();
}

void UIController::printOutputChannelScreen() const {
    char frequencyBuffer[14];
    lcd.setCursor(0, 0);
    lcd.print(F("Channel "));
    lcd.print(outputChannelIndex);
    lcd.print(F("           "));

    lcd.setCursor(0, 1);
    lcd.print(F("Set:   "));
    getOutputChannelFrequencyPadded(outputChannels[outputChannelIndex]->getSetFrequency(), frequencyBuffer);
    lcd.print(frequencyBuffer);

    lcd.setCursor(0, 2);
    lcd.print(F("Real:  "));
    getOutputChannelFrequencyPadded(outputChannels[outputChannelIndex]->getActualFrequency(), frequencyBuffer);
    lcd.print(frequencyBuffer);

    lcd.setCursor(0, 3);
    lcd.print(F("A|B|C: Back         "));

    lcd.setCursor(FREQUENCY_ADJUSTMENTS[frequencyAdjustmentIndex].col + 7, 1);
    lcd.cursor();
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
        sprintf(tmp, "%lu.%03lu.%03lu,%02lu", static_cast<uint32_t>(g2), static_cast<uint32_t>(g1),
                static_cast<uint32_t>(g0), static_cast<uint32_t>(decimal));
    } else if (g1 > 0) {
        sprintf(tmp, "%lu.%03lu,%02lu", static_cast<uint32_t>(g1), static_cast<uint32_t>(g0),
                static_cast<uint32_t>(decimal));
    } else {
        sprintf(tmp, "%lu,%02lu", static_cast<uint32_t>(g0), static_cast<uint32_t>(decimal));
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

    sprintf(out, "%02lu.%03lu.%03lu,%02lu", static_cast<uint32_t>(g2), static_cast<uint32_t>(g1),
            static_cast<uint32_t>(g0), static_cast<uint32_t>(decimal));
    // Result is always exactly 13 characters + NUL terminator.
    // Buffer must be at least 14 bytes.
}

OutputChannel* UIController::getOutputChannel() {
    if (outputChannelIndex < 0 || outputChannelIndex >= MAX_OUTPUT_CHANNELS) {
        Serial2.print(F("UIController::getOutputChannel(): Invalid output channel index "));
        Serial2.print(outputChannelIndex);
        Serial2.println(F(". Defaulting to 0."));
        outputChannelIndex = 0;
    }
    return outputChannels[outputChannelIndex];
}
