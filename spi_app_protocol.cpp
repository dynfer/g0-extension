#include "spi_app_protocol.h"

#include "ch.h"
#include "hal.h"

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
        chThdCreateStatic(spiAppThreadWa, sizeof(spiAppThreadWa),
                          NORMALPRIO + 1, threadEntry, this);
    }

    bool updateRequested() const
    {
        return updateRequestedFlag;
    }

private:
    static constexpr uint32_t appVersion = 8U;

    volatile bool updateRequestedFlag = false;

    const SPIConfig spi2Config = {
        false,
        true,
        nullptr,
        nullptr,
        GPIOB,
        GPIOB_SPI2_NSS,
        0U,
        SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
    };

    static THD_FUNCTION(threadEntry, arg)
    {
        static_cast<SpiAppProtocol *>(arg)->thread();
    }

    void thread()
    {
        protocol::AppFrame rxbuf = {};
        protocol::AppFrame txbuf = {};

        spiStart(&SPID2, &spi2Config);
        writeVersionResponse(txbuf, protocol::cmdReadVersion);

        while (true) {
            if (spiExchange(&SPID2, protocol::appFrameSize, txbuf.bytes, rxbuf.bytes) == MSG_OK) {
                buildResponse(rxbuf, txbuf);
            }
            else {
                spiStopTransfer(&SPID2, nullptr);
                initResponse(txbuf, protocol::cmdNop, protocol::resultBusy, 0U);
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
        txbuf.versionResponse.version = appVersion;
    }

    void writeAnalogResponse(protocol::AppFrame &txbuf, uint8_t command)
    {
        initResponse(txbuf, command, protocol::resultOk, protocol::analogPayloadLength);
        auto &response = txbuf.analogResponse;
        response.ready = analogVoltagesReady ? 1U : 0U;
        response.channelCount = analogInputCount;

        for (uint8_t index = 0U; index < analogInputCount; index++) {
            response.millivolts[index] = analogVoltagesMillivolts[index];
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

    void writeSentResponse(protocol::AppFrame &txbuf, uint8_t command, uint8_t input)
    {
        if (!validInput(input)) {
            initResponse(txbuf, command, protocol::resultRange, 0U);
            return;
        }

        const uint8_t index = input - 1U;
        SentFrame frame;
        bool ready;

        chSysLock();
        frame.data = digitalInputSentFrames[index].data;
        frame.status = digitalInputSentFrames[index].status;
        frame.crc = digitalInputSentFrames[index].crc;
        frame.nibbleCount = digitalInputSentFrames[index].nibbleCount;
        frame.valid = digitalInputSentFrames[index].valid;
        ready = digitalInputSentFrameReady[index];
        digitalInputSentFrameReady[index] = false;
        chSysUnlock();

        initResponse(txbuf, command, protocol::resultOk, protocol::sentPayloadLength);
        auto &response = txbuf.sentResponse;
        response.input = input;
        response.data = frame.data;
        response.status = frame.status;
        response.crc = frame.crc;
        response.nibbleCount = frame.nibbleCount;
        response.flags = (ready ? 1U : 0U) | (frame.valid ? (1U << 1U) : 0U);
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

    void buildResponse(const protocol::AppFrame &rxbuf, protocol::AppFrame &txbuf)
    {
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
            writeSentResponse(txbuf, command, rxbuf.indexedRequest.index);
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
            updateRequestedFlag = true;
            initResponse(txbuf, command, protocol::resultOk, 0U);
            break;
        default:
            initResponse(txbuf, command, protocol::resultInvalid, 0U);
            break;
        }
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

}  // namespace

void spi_app_protocol_start(void)
{
    spiAppProtocol.start();
}

bool spi_app_protocol_update_requested(void)
{
    return spiAppProtocol.updateRequested();
}
