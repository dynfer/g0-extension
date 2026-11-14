#include "spi_app_protocol.h"

#include "ch.hpp"
#include "hal.h"
#include "g0_firmware_version.h"

#include "analog.h"
#include "inputs.h"
#include "outputs.h"

namespace {

namespace protocol = g0_spi_protocol;

THD_WORKING_AREA(spiAppThreadWa, 384);

class SpiAppProtocol {
public:
    void start()
    {
        instance = this;
        chThdCreateStatic(spiAppThreadWa, sizeof(spiAppThreadWa),
                          NORMALPRIO + 1, threadEntry, this);
    }

    bool updateRequested() const
    {
        return updateRequestedFlag;
    }

private:
    static SpiAppProtocol *instance;
    chibios_rt::BinarySemaphore transferComplete{true};
    volatile bool transferFailed = false;
    volatile bool updateRequestedFlag = false;

    const SPIConfig spi2Config = {
        false,
        true,
        transferDone,
        transferError,
        GPIOB,
        GPIOB_SPI2_NSS,
        0U,
        SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
    };

    static THD_FUNCTION(threadEntry, arg)
    {
        static_cast<SpiAppProtocol *>(arg)->thread();
    }

    static void transferDone(SPIDriver *)
    {
        chSysLockFromISR();
        instance->transferComplete.signalI();
        chSysUnlockFromISR();
    }

    static void transferError(SPIDriver *)
    {
        chSysLockFromISR();
        instance->transferFailed = true;
        instance->transferComplete.signalI();
        chSysUnlockFromISR();
    }

    static bool partialTransfer()
    {
        chSysLock();
        const size_t remaining = dmaStreamGetTransactionSize(SPID2.dmarx);
        const bool partial = SPID2.state == SPI_ACTIVE && palReadLine(LINE_SPI2_NSS) &&
            remaining != 0U && remaining != protocol::appFrameSize;
        chSysUnlock();
        return partial;
    }

    void resetSpi()
    {
        while (!palReadLine(LINE_SPI2_NSS)) {
            chThdSleepMilliseconds(1);
        }
        chSysLock();
        // Reset first: stopping a truncated slave transfer can otherwise wait forever on BSY.
        rccResetSPI2();
        spiStopTransferI(&SPID2, nullptr);
        chSysUnlock();
        spiStop(&SPID2);
        spiStart(&SPID2, &spi2Config);
    }

    void thread()
    {
        protocol::AppFrame rxbuf = {};
        protocol::AppFrame txbuf = {};

        spiStart(&SPID2, &spi2Config);
        writeVersionResponse(txbuf, protocol::cmdReadVersion);
        protocol::seal(txbuf);

        while (true) {
            transferComplete.reset(true);
            transferFailed = false;
            spiStartExchange(&SPID2, protocol::appFrameSize, txbuf.bytes, rxbuf.bytes);
            while (transferComplete.wait(TIME_MS2I(1)) != MSG_OK) {
                if (partialTransfer()) {
                    transferFailed = true;
                    break;
                }
            }
            if (transferFailed || !buildResponse(rxbuf, txbuf)) {
                resetSpi();
                initResponse(txbuf, protocol::cmdNop, protocol::resultBusy, 0U);
                protocol::seal(txbuf);
            }
        }
    }

    static bool validInput(uint8_t input)
    {
        return (input >= 1U) && (input <= digitalInputCount);
    }

    static bool validOutput(uint8_t output)
    {
        return (output >= 1U) && (output <= outputCount);
    }

    static void clearFrame(protocol::AppFrame &frame)
    {
        for (uint8_t i = 0U; i < protocol::appFrameSize; i++) {
            frame.bytes[i] = 0U;
        }
    }

    static uint8_t flagsFromState(const DigitalInputState &state)
    {
        uint8_t flags = 0U;
        if (state.level) {
            flags |= 1U << 0U;
        }
        if (state.signalPresent) {
            flags |= 1U << 1U;
        }
        return flags;
    }

    void initResponse(protocol::AppFrame &txbuf,
                      uint8_t command,
                      uint8_t result,
                      uint8_t payloadLength)
    {
        clearFrame(txbuf);
        txbuf.responseHeader.status = updateRequestedFlag ? protocol::statusUpdateMode : protocol::statusReady;
        txbuf.responseHeader.result = result;
        txbuf.responseHeader.command = command;
        txbuf.responseHeader.payloadLength = payloadLength;
    }

    void writeVersionResponse(protocol::AppFrame &txbuf, uint8_t command)
    {
        initResponse(txbuf, command, protocol::resultOk, protocol::versionPayloadLength);
        txbuf.versionResponse.version = G0_FIRMWARE_VERSION;
        txbuf.versionResponse.protocolVersion = protocol::protocolVersion;
    }

    void writeAnalogResponse(protocol::AppFrame &txbuf, uint8_t command)
    {
        initResponse(txbuf, command, protocol::resultOk, protocol::analogPayloadLength);
        auto &response = txbuf.analogResponse;
        uint16_t millivolts[analogInputCount];
        response.ready = analogGetSnapshot(millivolts) ? 1U : 0U;
        response.channelCount = analogInputCount;

        for (uint8_t index = 0U; index < analogInputCount; index++) {
            response.millivolts[index] = millivolts[index];
        }
    }

    void writeDigitalResponse(protocol::AppFrame &txbuf, uint8_t command, uint8_t input)
    {
        if (!validInput(input)) {
            initResponse(txbuf, command, protocol::resultRange, 0U);
            return;
        }

        const DigitalInputState state = inputGetState(input);

        initResponse(txbuf, command, protocol::resultOk, protocol::digitalPayloadLength);
        auto &response = txbuf.digitalResponse;
        response.input = input;
        response.mode = static_cast<uint8_t>(state.mode);
        response.flags = flagsFromState(state);
        response.frequencyHz = state.frequencyHz;
        response.dutyPermille = state.dutyPermille;
        response.periodUs = state.periodUs;
        response.highTimeUs = state.highTimeUs;
        response.captureCount = state.captureCount;
    }

    void writeAllDigitalResponse(protocol::AppFrame &txbuf, uint8_t command)
    {
        initResponse(txbuf, command, protocol::resultOk, protocol::digitalAllPayloadLength);
        auto &response = txbuf.digitalAllResponse;
        response.inputCount = digitalInputCount;

        for (uint8_t index = 0U; index < digitalInputCount; index++) {
            const DigitalInputState state = inputGetState(index + 1U);
            const uint32_t frequency =
                (state.frequencyHz > 0xFFFFU) ? 0xFFFFU : state.frequencyHz;
            auto &input = response.inputs[index];

            input.mode = static_cast<uint8_t>(state.mode);
            input.flags = flagsFromState(state);
            input.frequencyHz = static_cast<uint16_t>(frequency);
            input.dutyPermille = state.dutyPermille;
        }
    }

    void writeOutputResponse(protocol::AppFrame &txbuf, uint8_t command, uint8_t output)
    {
        if (!validOutput(output)) {
            initResponse(txbuf, command, protocol::resultRange, 0U);
            return;
        }

        const OutputState state = outputGetState(output);

        initResponse(txbuf, command, protocol::resultOk, protocol::outputPayloadLength);
        auto &response = txbuf.outputResponse;
        response.output = output;
        response.frequencyHz = state.frequencyHz;
        response.duty = state.duty;
    }

    bool buildResponse(const protocol::AppFrame &rxbuf, protocol::AppFrame &txbuf)
    {
        if (!protocol::validFrame(rxbuf)) {
            for (uint8_t output = 1U; output <= outputCount; output++) {
                outputDisable(output);
            }
            return false;
        }
        outputCommunicationReceived();
        const uint8_t command = rxbuf.commandRequest.command;

        switch (command) {
        case protocol::cmdNop:
            initResponse(txbuf, command, protocol::resultOk, 0U);
            break;
        case protocol::cmdReadVersion:
            writeVersionResponse(txbuf, command);
            break;
        case protocol::cmdReadAnalog:
            writeAnalogResponse(txbuf, command);
            break;
        case protocol::cmdReadDigital:
            writeDigitalResponse(txbuf, command, rxbuf.indexedRequest.index);
            break;
        case protocol::cmdReadDigitalAll:
            writeAllDigitalResponse(txbuf, command);
            break;
        case protocol::cmdReadSent:
            initResponse(txbuf, command, protocol::resultInvalid, 0U);
            break;
        case protocol::cmdSetOutput:
            setOutput(txbuf, rxbuf);
            break;
        case protocol::cmdDisableOutput:
            disableOutput(txbuf, rxbuf);
            break;
        case protocol::cmdReadOutput:
            writeOutputResponse(txbuf, command, rxbuf.indexedRequest.index);
            break;
        case protocol::cmdSetInputMode:
            setInputMode(txbuf, rxbuf);
            break;
        case protocol::cmdEnterUpdate:
            for (uint8_t output = 1U; output <= outputCount; output++) {
                outputDisable(output);
            }
            updateRequestedFlag = true;
            initResponse(txbuf, command, protocol::resultOk, 0U);
            break;
        default:
            initResponse(txbuf, command, protocol::resultInvalid, 0U);
            break;
        }
        protocol::seal(txbuf);
        return true;
    }

    void setOutput(protocol::AppFrame &txbuf, const protocol::AppFrame &rxbuf)
    {
        const auto &request = rxbuf.setOutputRequest;

        if (outputSet(request.output, request.frequencyHz, request.duty)) {
            writeOutputResponse(txbuf, request.command, request.output);
        }
        else {
            initResponse(txbuf, request.command, protocol::resultRange, 0U);
        }
    }

    void disableOutput(protocol::AppFrame &txbuf, const protocol::AppFrame &rxbuf)
    {
        const auto &request = rxbuf.indexedRequest;

        if (validOutput(request.index)) {
            outputDisable(request.index);
            writeOutputResponse(txbuf, request.command, request.index);
        }
        else {
            initResponse(txbuf, request.command, protocol::resultRange, 0U);
        }
    }

    void setInputMode(protocol::AppFrame &txbuf, const protocol::AppFrame &rxbuf)
    {
        const auto &request = rxbuf.setInputModeRequest;

        if (inputSetMode(request.input, static_cast<DigitalInputMode>(request.mode))) {
            writeDigitalResponse(txbuf, request.command, request.input);
        }
        else {
            initResponse(txbuf, request.command, protocol::resultRange, 0U);
        }
    }
};

SpiAppProtocol spiAppProtocol;
SpiAppProtocol *SpiAppProtocol::instance = &spiAppProtocol;

}  // namespace

void spi_app_protocol_start(void)
{
    spiAppProtocol.start();
}

bool spi_app_protocol_update_requested(void)
{
    return spiAppProtocol.updateRequested();
}
