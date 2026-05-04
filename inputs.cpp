#include "inputs.h"

#include "ch.hpp"
#include "hal.h"
#include "ch.h"

volatile DigitalInputState digitalInputStates[digitalInputCount] = {};
volatile SentFrame digitalInputSentFrames[digitalInputCount] = {};
volatile bool digitalInputSentFrameReady[digitalInputCount] = {};

namespace {

THD_WORKING_AREA(inputThreadWa, 512);

class DigitalInputs {
public:
    void init()
    {
        chThdCreateStatic(inputThreadWa, sizeof(inputThreadWa),
                          NORMALPRIO + 1, threadEntry, this);

        for (uint8_t index = 0U; index < digitalInputCount; index++) {
            inputs[index].line = lines[index];
            inputs[index].driver = drivers[index];
            inputs[index].mode = DigitalInputMode::Digital;
            inputs[index].lastCaptureSystemTime = chVTGetSystemTimeX();

            digitalInputStates[index].mode = DigitalInputMode::Digital;
            digitalInputStates[index].level = readLine(index);
            digitalInputStates[index].signalPresent = true;

            configs[index].width_cb = widthCallback;
            configs[index].period_cb = periodCallback;
            configs[index].overflow_cb = overflowCallback;

            icuStart(inputs[index].driver, &configs[index]);
            icuStartCapture(inputs[index].driver);
            icuEnableNotifications(inputs[index].driver);
        }
    }

    bool setMode(uint8_t input, DigitalInputMode mode)
    {
        if (!isValid(input) || !isValid(mode)) {
            return false;
        }

        chSysLock();
        inputs[input - 1U].mode = mode;
        resetModeStateI(input - 1U);
        chSysUnlock();

        return true;
    }

    bool read(uint8_t input) const
    {
        if (!isValid(input)) {
            return false;
        }

        return palReadLine(inputs[input - 1U].line) == PAL_HIGH;
    }

    DigitalInputState getState(uint8_t input)
    {
        if (!isValid(input)) {
            return {};
        }

        DigitalInputState state;
        const uint8_t index = input - 1U;

        chSysLock();
        refreshGlobalI(index);
        state.mode = digitalInputStates[index].mode;
        state.level = digitalInputStates[index].level;
        state.signalPresent = digitalInputStates[index].signalPresent;
        state.frequencyHz = digitalInputStates[index].frequencyHz;
        state.dutyPermille = digitalInputStates[index].dutyPermille;
        state.periodUs = digitalInputStates[index].periodUs;
        state.highTimeUs = digitalInputStates[index].highTimeUs;
        state.captureCount = digitalInputStates[index].captureCount;
        chSysUnlock();

        return state;
    }

    uint8_t readCaptures(uint8_t input,
                         DigitalInputCapture *captures,
                         uint8_t maxCaptures)
    {
        if (!isValid(input) || (captures == nullptr) || (maxCaptures == 0U)) {
            return 0U;
        }

        const uint8_t index = input - 1U;
        uint8_t copied;

        chSysLock();
        copied = inputs[index].capturesBuffered;
        if (copied > maxCaptures) {
            copied = maxCaptures;
        }

        const uint8_t start = static_cast<uint8_t>(
            (inputs[index].captureWriteIndex + digitalInputCaptureBufferSize -
             inputs[index].capturesBuffered) % digitalInputCaptureBufferSize);

        for (uint8_t i = 0U; i < copied; i++) {
            const uint8_t captureIndex =
                static_cast<uint8_t>((start + i) % digitalInputCaptureBufferSize);
            captures[i] = inputs[index].captures[captureIndex];
        }

        inputs[index].capturesBuffered = 0U;
        chSysUnlock();

        return copied;
    }

    uint8_t readSentFrames(uint8_t input, SentFrame *frames, uint8_t maxFrames)
    {
        if (!isValid(input) || (frames == nullptr) || (maxFrames == 0U)) {
            return 0U;
        }

        const uint8_t index = input - 1U;
        uint8_t copied;

        chSysLock();
        copied = inputs[index].sentBuffered;
        if (copied > maxFrames) {
            copied = maxFrames;
        }

        const uint8_t start = static_cast<uint8_t>(
            (inputs[index].sentWriteIndex + sentFrameBufferSize -
             inputs[index].sentBuffered) % sentFrameBufferSize);

        for (uint8_t i = 0U; i < copied; i++) {
            const uint8_t frameIndex =
                static_cast<uint8_t>((start + i) % sentFrameBufferSize);
            frames[i] = inputs[index].sentFrames[frameIndex];
        }

        inputs[index].sentBuffered = 0U;
        chSysUnlock();

        return copied;
    }

private:
    static constexpr icufreq_t icuFrequencyHz = 1000000U;
    static constexpr sysinterval_t signalTimeout = TIME_MS2I(100U);
    static constexpr uint8_t sentSyncTicks = 56U;
    static constexpr uint8_t sentNibbleOffsetTicks = 12U;
    static constexpr uint8_t sentMinNibbles = 6U;
    static constexpr uint8_t sentMaxNibbles = 8U;

    struct SentDecoder {
        bool synced;
        uint16_t tickUs;
        uint8_t nibbleCount;
        uint8_t nibbles[sentMaxNibbles];
    };

    struct InputCapture {
        ioline_t line;
        ICUDriver *driver;
        volatile DigitalInputMode mode;
        volatile bool signalPresent;
        volatile uint32_t frequencyHz;
        volatile uint16_t dutyPermille;
        volatile uint16_t periodUs;
        volatile uint16_t highTimeUs;
        volatile systime_t lastCaptureSystemTime;
        volatile uint32_t captureCount;
        volatile uint8_t rawWriteIndex;
        volatile uint8_t rawReadIndex;
        volatile uint8_t rawBuffered;
        volatile uint8_t captureWriteIndex;
        volatile uint8_t capturesBuffered;
        volatile uint8_t sentWriteIndex;
        volatile uint8_t sentBuffered;
        DigitalInputCapture rawCaptures[digitalInputCaptureBufferSize];
        DigitalInputCapture captures[digitalInputCaptureBufferSize];
        SentFrame sentFrames[sentFrameBufferSize];
        SentDecoder sent;
    };

    const ioline_t lines[digitalInputCount] = {
        LINE_TIM14_CH1,
        LINE_TIM15_CH1,
        LINE_TIM16_CH1,
        LINE_TIM17_CH1,
    };

    ICUDriver *const drivers[digitalInputCount] = {
        &ICUD14,
        &ICUD15,
        &ICUD16,
        &ICUD17,
    };

    InputCapture inputs[digitalInputCount] = {};
    ICUConfig configs[digitalInputCount] = {
        {ICU_INPUT_ACTIVE_HIGH, icuFrequencyHz, nullptr, nullptr, nullptr, ICU_CHANNEL_1, 0U, 0xFFFFU},
        {ICU_INPUT_ACTIVE_HIGH, icuFrequencyHz, nullptr, nullptr, nullptr, ICU_CHANNEL_1, 0U, 0xFFFFU},
        {ICU_INPUT_ACTIVE_HIGH, icuFrequencyHz, nullptr, nullptr, nullptr, ICU_CHANNEL_1, 0U, 0xFFFFU},
        {ICU_INPUT_ACTIVE_HIGH, icuFrequencyHz, nullptr, nullptr, nullptr, ICU_CHANNEL_1, 0U, 0xFFFFU},
    };

    chibios_rt::BinarySemaphore captureSem{true};
    static DigitalInputs *instance;

    static bool isValid(uint8_t input)
    {
        return (input >= 1U) && (input <= digitalInputCount);
    }

    static bool isValid(DigitalInputMode mode)
    {
        return (mode == DigitalInputMode::Digital) ||
               (mode == DigitalInputMode::Frequency) ||
               (mode == DigitalInputMode::Sent);
    }

    bool readLine(uint8_t index) const
    {
        return palReadLine(inputs[index].line) == PAL_HIGH;
    }

    static THD_FUNCTION(threadEntry, arg)
    {
        static_cast<DigitalInputs *>(arg)->thread();
    }

    static void widthCallback(ICUDriver *driver)
    {
        instance->onWidth(driver);
    }

    static void periodCallback(ICUDriver *driver)
    {
        instance->onPeriod(driver);
    }

    static void overflowCallback(ICUDriver *driver)
    {
        instance->onOverflow(driver);
    }

    void thread()
    {
        chRegSetThreadName("Inputs");

        while (true) {
            captureSem.wait(TIME_MS2I(10U));
            drainCaptures();
            refreshAllGlobals();
        }
    }

    void drainCaptures()
    {
        bool processed;
        do {
            processed = false;
            for (uint8_t index = 0U; index < digitalInputCount; index++) {
                DigitalInputCapture capture;
                while (popRawCapture(index, capture)) {
                    processCapture(index, capture);
                    processed = true;
                }
            }
        } while (processed);
    }

    uint8_t indexFromDriver(ICUDriver *driver) const
    {
        for (uint8_t index = 0U; index < digitalInputCount; index++) {
            if (inputs[index].driver == driver) {
                return index;
            }
        }

        return digitalInputCount;
    }

    void onWidth(ICUDriver *driver)
    {
        const uint8_t index = indexFromDriver(driver);
        if (index < digitalInputCount) {
            inputs[index].highTimeUs = static_cast<uint16_t>(icuGetWidthX(driver));
        }
    }

    void onPeriod(ICUDriver *driver)
    {
        const uint8_t index = indexFromDriver(driver);
        if (index >= digitalInputCount) {
            return;
        }

        const uint16_t periodUs = static_cast<uint16_t>(icuGetPeriodX(driver));
        const uint16_t highTimeUs = inputs[index].highTimeUs;

        pushRawCaptureI(inputs[index], periodUs, highTimeUs);
        captureSem.signalI();
    }

    void onOverflow(ICUDriver *driver)
    {
        const uint8_t index = indexFromDriver(driver);
        if (index >= digitalInputCount) {
            return;
        }

        inputs[index].signalPresent = false;
        inputs[index].frequencyHz = 0U;
        inputs[index].dutyPermille = 0U;
        digitalInputStates[index].signalPresent = false;
        digitalInputStates[index].frequencyHz = 0U;
        digitalInputStates[index].dutyPermille = 0U;
    }

    static void pushRawCaptureI(InputCapture &input, uint16_t periodUs, uint16_t highTimeUs)
    {
        input.rawCaptures[input.rawWriteIndex] = {periodUs, highTimeUs};
        input.rawWriteIndex =
            static_cast<uint8_t>((input.rawWriteIndex + 1U) % digitalInputCaptureBufferSize);

        if (input.rawBuffered < digitalInputCaptureBufferSize) {
            input.rawBuffered++;
        }
        else {
            input.rawReadIndex =
                static_cast<uint8_t>((input.rawReadIndex + 1U) % digitalInputCaptureBufferSize);
        }
    }

    bool popRawCapture(uint8_t index, DigitalInputCapture &capture)
    {
        chSysLock();
        if (inputs[index].rawBuffered == 0U) {
            chSysUnlock();
            return false;
        }

        capture = inputs[index].rawCaptures[inputs[index].rawReadIndex];
        inputs[index].rawReadIndex =
            static_cast<uint8_t>((inputs[index].rawReadIndex + 1U) %
                                 digitalInputCaptureBufferSize);
        inputs[index].rawBuffered--;
        chSysUnlock();

        return true;
    }

    void processCapture(uint8_t index, const DigitalInputCapture &capture)
    {
        InputCapture &input = inputs[index];
        const DigitalInputMode mode = input.mode;

        chSysLock();
        input.signalPresent = true;
        input.periodUs = capture.periodUs;
        input.highTimeUs = capture.highTimeUs;
        input.captureCount++;
        input.lastCaptureSystemTime = chVTGetSystemTimeX();

        if (capture.periodUs != 0U) {
            input.frequencyHz = icuFrequencyHz / capture.periodUs;
            input.dutyPermille =
                static_cast<uint16_t>((static_cast<uint32_t>(capture.highTimeUs) * 1000U) /
                                      capture.periodUs);
        }
        else {
            input.frequencyHz = 0U;
            input.dutyPermille = 0U;
        }

        if (mode == DigitalInputMode::Frequency) {
            appendCapture(input, capture.periodUs, capture.highTimeUs);
        }
        else if (mode == DigitalInputMode::Sent) {
            processSentCapture(index, capture);
        }

        copyStateToGlobalI(index, false);
        chSysUnlock();
    }

    static void appendCapture(InputCapture &input, uint16_t periodUs, uint16_t highTimeUs)
    {
        input.captures[input.captureWriteIndex] = {periodUs, highTimeUs};
        input.captureWriteIndex =
            static_cast<uint8_t>((input.captureWriteIndex + 1U) %
                                 digitalInputCaptureBufferSize);

        if (input.capturesBuffered < digitalInputCaptureBufferSize) {
            input.capturesBuffered++;
        }
    }

    static bool isSentSyncPulse(uint16_t periodUs)
    {
        return periodUs >= 100U;
    }

    static void resetSentDecoder(SentDecoder &decoder, uint16_t syncPeriodUs)
    {
        decoder.synced = true;
        decoder.tickUs = syncPeriodUs / sentSyncTicks;
        if (decoder.tickUs == 0U) {
            decoder.tickUs = 1U;
        }
        decoder.nibbleCount = 0U;
    }

    void processSentCapture(uint8_t index, const DigitalInputCapture &capture)
    {
        InputCapture &input = inputs[index];

        if (isSentSyncPulse(capture.periodUs)) {
            resetSentDecoder(input.sent, capture.periodUs);
            return;
        }

        if (!input.sent.synced || (input.sent.tickUs == 0U)) {
            return;
        }

        uint16_t ticks = (capture.periodUs + (input.sent.tickUs / 2U)) / input.sent.tickUs;
        if ((ticks < sentNibbleOffsetTicks) ||
            (ticks > (sentNibbleOffsetTicks + 15U)) ||
            (input.sent.nibbleCount >= sentMaxNibbles)) {
            input.sent.synced = false;
            return;
        }

        input.sent.nibbles[input.sent.nibbleCount++] =
            static_cast<uint8_t>(ticks - sentNibbleOffsetTicks);

        if (input.sent.nibbleCount >= sentMinNibbles) {
            SentFrame frame = {};
            frame.status = input.sent.nibbles[0];
            frame.nibbleCount = input.sent.nibbleCount;

            for (uint8_t i = 1U; i < (input.sent.nibbleCount - 1U); i++) {
                frame.data = (frame.data << 4U) | input.sent.nibbles[i];
            }

            frame.crc = input.sent.nibbles[input.sent.nibbleCount - 1U];
            frame.valid = true;
            appendSentFrame(index, frame);
            input.sent.synced = false;
        }
    }

    void appendSentFrame(uint8_t index, const SentFrame &frame)
    {
        InputCapture &input = inputs[index];

        input.sentFrames[input.sentWriteIndex] = frame;
        input.sentWriteIndex =
            static_cast<uint8_t>((input.sentWriteIndex + 1U) % sentFrameBufferSize);

        if (input.sentBuffered < sentFrameBufferSize) {
            input.sentBuffered++;
        }

        digitalInputSentFrames[index].data = frame.data;
        digitalInputSentFrames[index].status = frame.status;
        digitalInputSentFrames[index].crc = frame.crc;
        digitalInputSentFrames[index].nibbleCount = frame.nibbleCount;
        digitalInputSentFrames[index].valid = frame.valid;
        digitalInputSentFrameReady[index] = true;
    }

    void resetModeStateI(uint8_t index)
    {
        InputCapture &input = inputs[index];

        input.signalPresent = false;
        input.frequencyHz = 0U;
        input.dutyPermille = 0U;
        input.periodUs = 0U;
        input.highTimeUs = 0U;
        input.captureCount = 0U;
        input.rawWriteIndex = 0U;
        input.rawReadIndex = 0U;
        input.rawBuffered = 0U;
        input.captureWriteIndex = 0U;
        input.capturesBuffered = 0U;
        input.sentWriteIndex = 0U;
        input.sentBuffered = 0U;
        input.sent = {};

        digitalInputSentFrames[index].data = 0U;
        digitalInputSentFrames[index].status = 0U;
        digitalInputSentFrames[index].crc = 0U;
        digitalInputSentFrames[index].nibbleCount = 0U;
        digitalInputSentFrames[index].valid = false;
        digitalInputSentFrameReady[index] = false;
        refreshGlobalI(index);
    }

    void refreshAllGlobals()
    {
        chSysLock();
        for (uint8_t index = 0U; index < digitalInputCount; index++) {
            refreshGlobalI(index);
        }
        chSysUnlock();
    }

    void refreshGlobalI(uint8_t index)
    {
        bool timedOut = false;
        if (inputs[index].mode != DigitalInputMode::Digital) {
            timedOut = chVTTimeElapsedSinceX(inputs[index].lastCaptureSystemTime) > signalTimeout;
            if (timedOut) {
                inputs[index].signalPresent = false;
                inputs[index].frequencyHz = 0U;
                inputs[index].dutyPermille = 0U;
            }
        }

        copyStateToGlobalI(index, timedOut);
    }

    void copyStateToGlobalI(uint8_t index, bool timedOut)
    {
        const bool digitalMode = inputs[index].mode == DigitalInputMode::Digital;

        digitalInputStates[index].mode = inputs[index].mode;
        digitalInputStates[index].level = readLine(index);
        digitalInputStates[index].signalPresent =
            digitalMode || (!timedOut && inputs[index].signalPresent);
        digitalInputStates[index].frequencyHz = timedOut ? 0U : inputs[index].frequencyHz;
        digitalInputStates[index].dutyPermille = timedOut ? 0U : inputs[index].dutyPermille;
        digitalInputStates[index].periodUs = inputs[index].periodUs;
        digitalInputStates[index].highTimeUs = inputs[index].highTimeUs;
        digitalInputStates[index].captureCount = inputs[index].captureCount;
    }
};

DigitalInputs digitalInputs;
DigitalInputs *DigitalInputs::instance = &digitalInputs;

}  // namespace

void inputInit(void)
{
    digitalInputs.init();
}

bool inputSetMode(uint8_t input, DigitalInputMode mode)
{
    return digitalInputs.setMode(input, mode);
}

bool inputRead(uint8_t input)
{
    return digitalInputs.read(input);
}

DigitalInputState inputGetState(uint8_t input)
{
    return digitalInputs.getState(input);
}

uint8_t inputReadCaptures(uint8_t input,
                          DigitalInputCapture *captures,
                          uint8_t maxCaptures)
{
    return digitalInputs.readCaptures(input, captures, maxCaptures);
}

uint8_t inputReadSentFrames(uint8_t input, SentFrame *frames, uint8_t maxFrames)
{
    return digitalInputs.readSentFrames(input, frames, maxFrames);
}
