#include "ch.hpp"
#include "hal.h"
#include <cstdio>

// Compile the real slave code against HAL fixtures; inspect callback-only paths too.
#define private public
#include "../outputs.cpp"
#include "../inputs.cpp"
#include "../analog.cpp"
#include "../spi_app_protocol.cpp"
#undef private

namespace wire = g0_spi_protocol;

static void testOutputs()
{
    outputInit();
    assert(fakeStartupGlitches == 0);
    for (unsigned i = 0; i < outputCount; i++) assert(!fakeLevels[i]);
    assert(!outputSet(0, 100, 5000));
    assert(!outputSet(1, 501, 5000));
    assert(!outputSet(1, 100, 10001));

    assert(outputSet(1, 1, 5000));
    for (unsigned ms = 100; ms <= 2100; ms += 100) {
        fakeAdvance(TIME_MS2I(ms));
        outputCommunicationReceived();
        assert(outputSet(1, 1, ms % 200 == 0 ? 5000 : 5100));
        if (ms == 600 || ms == 1600) assert(!fakeLevels[0]);
    }
    assert(fakeFalls >= 2); // Updates cannot stretch HIGH indefinitely.
    assert(outputSet(1, 0, outputDutyMax));
    assert(outputSet(2, 500, 5000));
    fakeAdvance(TIME_MS2I(2599));
    assert(fakeLevels[0]);
    fakeAdvance(TIME_MS2I(2600));
    for (unsigned i = 0; i < outputCount; i++) {
        assert(!fakeLevels[i]);
        assert(outputGetState(i + 1).duty == 0);
    }
    fakeAdvance(TIME_MS2I(4000));
    for (unsigned i = 0; i < outputCount; i++) assert(!fakeLevels[i]);
}

static void edge(unsigned index, uint32_t us, bool high)
{
    auto line = inputs[index].line;
    fakeNow = us / 10U;
    timerBase = us & 0xFFFF0000U;
    GPTD6.tim->CNT = us & 0xFFFFU;
    GPTD6.tim->SR = 0;
    fakeLevels[line] = high;
    assert(fakeEvents[line] && fakeCallbacks[line]);
    fakeCallbacks[line](fakeCallbackArgs[line]);
    assert(fakeLocks == 0);
}

static void testInputs()
{
    inputInit();
    assert(GPTD6.config->frequency == 1000000);
    assert(!inputSetMode(1, DigitalInputMode::Sent));
    assert(!inputSetMode(0, DigitalInputMode::Frequency));
    for (unsigned index = 0; index < digitalInputCount; index++) {
        for (unsigned repeat = 0; repeat < 2; repeat++) {
            assert(inputSetMode(index + 1, DigitalInputMode::Frequency));
            edge(index, 65000, true);
            edge(index, 65250, false);
            assert(!inputGetState(index + 1).signalPresent);
            edge(index, 66000, true); // Cross the 16-bit timer boundary.
            auto state = inputGetState(index + 1);
            assert(state.signalPresent && state.frequencyHz == 1000);
            assert(state.periodUs == 1000 && state.highTimeUs == 250 && state.dutyPermille == 250);
            assert(inputSetMode(index + 1, DigitalInputMode::Frequency));
            assert(inputGetState(index + 1).captureCount == 1); // Duplicate config keeps captures.
            fakeNow += TIME_MS2I(100);
            state = inputGetState(index + 1);
            assert(!state.signalPresent && state.frequencyHz == 0 && state.periodUs == 0);
            assert(inputSetMode(index + 1, DigitalInputMode::Digital));
            assert(inputGetState(index + 1).level && inputGetState(index + 1).signalPresent);
            assert(!fakeEvents[inputs[index].line]);
        }
    }
    assert(inputSetMode(1, DigitalInputMode::Frequency));
    edge(0, 100000, true);
    edge(0, 600000, false);
    edge(0, 1100000, true);
    auto state = inputGetState(1);
    assert(state.signalPresent && state.frequencyHz == 1 && state.dutyPermille == 500);
    assert(state.periodUs == 0xFFFF && state.highTimeUs == 0xFFFF);
    fakeNow += TIME_MS2I(1000);
    assert(inputGetState(1).signalPresent);
    fakeNow += TIME_MS2I(1000);
    assert(!inputGetState(1).signalPresent);

    timerBase = 0xFFFF0000U;
    GPTD6.tim->SR = STM32_TIM_SR_UIF;
    GPTD6.tim->CNT = 20;
    assert(timestampI() == 20); // Pending overflow and 32-bit wrap.
    GPTD6.tim->SR = 0;
    timerOverflow(&GPTD6);
    assert(timestampI() == 20);
}

static void testAnalog()
{
    uint16_t values[analogInputCount];
    assert(!analogGetSnapshot(values));
    startSampling();
    adcsample_t samples[analogInputCount * 32];
    std::fill(std::begin(samples), std::end(samples), 4095);
    sampler.processSamples(samples);
    assert(analogGetSnapshot(values));
    for (auto value : values) assert(value == 3300);
    fakeNow += TIME_MS2I(250);
    assert(!analogGetSnapshot(values));
    for (auto value : values) assert(value == 0);
    sampler.processSamples(samples);
    sampler.conversionGroup.error(&ADCD1, 1);
    ADCD1.state = ADC_READY;
    sampler.processSamples(samples); // A queued completion cannot resurrect failed samples.
    assert(!analogGetSnapshot(values));
    ADCD1.complete = false;
    sampler.conversionGroup.done(&ADCD1);
    ADCD1.complete = true;
    sampler.conversionGroup.done(&ADCD1);
    assert(fakeLocks == 0);
}

static void testSpi()
{
    fakeNow = 0;
    outputInit();
    wire::AppFrame request = {}, response = {};
    request.setOutputRequest = {wire::cmdSetOutput, 1, 100, 5000};
    wire::seal(request);
    for (unsigned bit = 0; bit < wire::appFrameSize * 8; bit++) {
        auto corrupt = request;
        corrupt.bytes[bit / 8] ^= 1U << (bit % 8);
        assert(!spiAppProtocol.buildResponse(corrupt, response));
        for (unsigned i = 1; i <= outputCount; i++) assert(outputGetState(i).duty == 0);
    }
    assert(spiAppProtocol.buildResponse(request, response));
    assert(wire::validFrame(response));
    assert(outputGetState(1).duty == 5000);
    request = {};
    request.bytes[0] = wire::cmdReadVersion;
    wire::seal(request);
    assert(spiAppProtocol.buildResponse(request, response));
    assert(response.versionResponse.version == G0_FIRMWARE_VERSION);
    assert(response.versionResponse.protocolVersion == wire::protocolVersion);
    request.bytes[0] = wire::cmdEnterUpdate;
    wire::seal(request);
    assert(spiAppProtocol.buildResponse(request, response));
    assert(wire::validFrame(response));
    assert(response.responseHeader.command == wire::cmdEnterUpdate);
    assert(response.responseHeader.status == wire::statusReady);
    assert(response.responseHeader.result == wire::resultInvalid);
    request.bytes[0] = wire::cmdReadSent;
    wire::seal(request);
    assert(spiAppProtocol.buildResponse(request, response));
    assert(response.responseHeader.result == wire::resultInvalid);
    fakeAdvance(TIME_MS2I(500));
    assert(outputGetState(1).duty == 0);

    fakeLevels[LINE_SPI2_NSS] = true;
    spiStartExchange(&SPID2, wire::appFrameSize, request.bytes, response.bytes);
    assert(!spiAppProtocol.partialTransfer()); // Idle, no request yet.
    SPID2.remaining = 1; // A 35-byte transaction ended.
    fakeLevels[LINE_SPI2_NSS] = false;
    assert(!spiAppProtocol.partialTransfer()); // Never reset a transfer with CS asserted.
    fakeLevels[LINE_SPI2_NSS] = true;
    assert(spiAppProtocol.partialTransfer());
    spiAppProtocol.resetSpi();
    assert(fakeSpiResets == 1);
    spiStartExchange(&SPID2, wire::appFrameSize, request.bytes, response.bytes);
    assert(SPID2.remaining == wire::appFrameSize);
    SPID2.remaining = 0;
    assert(!spiAppProtocol.partialTransfer());
    SPID2.state = SPI_READY;
    spiAppProtocol.transferDone(&SPID2);
    assert(spiAppProtocol.transferComplete.wait() == MSG_OK);
    spiAppProtocol.transferError(&SPID2);
    assert(spiAppProtocol.transferFailed);
    assert(spiAppProtocol.transferComplete.wait() == MSG_OK && fakeLocks == 0);
}

int main()
{
    assert(wire::crc16(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0x29B1);
    testOutputs();
    testInputs();
    testAnalog();
    testSpi();
    std::puts("G0 slave regressions passed");
}
