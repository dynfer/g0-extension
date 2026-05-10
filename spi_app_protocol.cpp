#include "spi_app_protocol.h"

#include "ch.h"
#include "hal.h"

#include "analog.h"
#include "inputs.h"
#include "outputs.h"

namespace {

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
    static constexpr uint32_t appVersion = 7U;

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
        uint8_t rxbuf[spiAppFrameSize] = {};
        uint8_t txbuf[spiAppFrameSize] = {};

        spiStart(&SPID2, &spi2Config);
        writeVersionResponse(txbuf, spiCmdReadVersion);

        while (true) {
            if (spiExchange(&SPID2, spiAppFrameSize, txbuf, rxbuf) == MSG_OK) {
                buildResponse(rxbuf, txbuf);
            }
            else {
                spiStopTransfer(&SPID2, nullptr);
                initResponse(txbuf, spiCmdNop, spiResultBusy, 0U);
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

    static void clearFrame(uint8_t *frame)
    {
        for (uint8_t i = 0U; i < spiAppFrameSize; i++) {
            frame[i] = 0U;
        }
    }

    static void putU16(uint8_t *buffer, uint8_t offset, uint16_t value)
    {
        buffer[offset] = static_cast<uint8_t>(value);
        buffer[offset + 1U] = static_cast<uint8_t>(value >> 8U);
    }

    static void putU32(uint8_t *buffer, uint8_t offset, uint32_t value)
    {
        buffer[offset] = static_cast<uint8_t>(value);
        buffer[offset + 1U] = static_cast<uint8_t>(value >> 8U);
        buffer[offset + 2U] = static_cast<uint8_t>(value >> 16U);
        buffer[offset + 3U] = static_cast<uint8_t>(value >> 24U);
    }

    static uint16_t getU16(const uint8_t *buffer, uint8_t offset)
    {
        return static_cast<uint16_t>(buffer[offset]) |
               static_cast<uint16_t>(static_cast<uint16_t>(buffer[offset + 1U]) << 8U);
    }

    static uint32_t getU32(const uint8_t *buffer, uint8_t offset)
    {
        return static_cast<uint32_t>(buffer[offset]) |
               (static_cast<uint32_t>(buffer[offset + 1U]) << 8U) |
               (static_cast<uint32_t>(buffer[offset + 2U]) << 16U) |
               (static_cast<uint32_t>(buffer[offset + 3U]) << 24U);
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

    void initResponse(uint8_t *txbuf,
                      uint8_t command,
                      uint8_t result,
                      uint8_t payloadLength)
    {
        clearFrame(txbuf);
        txbuf[0] = updateRequestedFlag ? spiStatusUpdateMode : spiStatusReady;
        txbuf[1] = result;
        txbuf[2] = command;
        txbuf[3] = payloadLength;
    }

    void writeVersionResponse(uint8_t *txbuf, uint8_t command)
    {
        initResponse(txbuf, command, spiResultOk, 4U);
        putU32(txbuf, spiAppHeaderSize, appVersion);
    }

    void writeAnalogResponse(uint8_t *txbuf, uint8_t command)
    {
        initResponse(txbuf, command, spiResultOk, 26U);
        txbuf[spiAppHeaderSize] = analogVoltagesReady ? 1U : 0U;
        txbuf[spiAppHeaderSize + 1U] = analogInputCount;

        for (uint8_t index = 0U; index < analogInputCount; index++) {
            putU16(txbuf, static_cast<uint8_t>(spiAppHeaderSize + 2U + (index * 2U)),
                   analogVoltagesMillivolts[index]);
        }
    }

    void writeDigitalResponse(uint8_t *txbuf, uint8_t command, uint8_t input)
    {
        if (!validInput(input)) {
            initResponse(txbuf, command, spiResultRange, 0U);
            return;
        }

        const DigitalInputState state = inputGetState(input);

        initResponse(txbuf, command, spiResultOk, 17U);
        txbuf[4] = input;
        txbuf[5] = static_cast<uint8_t>(state.mode);
        txbuf[6] = flagsFromState(state);
        putU32(txbuf, 7U, state.frequencyHz);
        putU16(txbuf, 11U, state.dutyPermille);
        putU16(txbuf, 13U, state.periodUs);
        putU16(txbuf, 15U, state.highTimeUs);
        putU32(txbuf, 17U, state.captureCount);
    }

    void writeAllDigitalResponse(uint8_t *txbuf, uint8_t command)
    {
        initResponse(txbuf, command, spiResultOk, 25U);
        txbuf[4] = digitalInputCount;

        for (uint8_t index = 0U; index < digitalInputCount; index++) {
            const DigitalInputState state = inputGetState(index + 1U);
            const uint8_t offset = static_cast<uint8_t>(5U + (index * 6U));
            const uint32_t frequency =
                (state.frequencyHz > 0xFFFFU) ? 0xFFFFU : state.frequencyHz;

            txbuf[offset] = static_cast<uint8_t>(state.mode);
            txbuf[offset + 1U] = flagsFromState(state);
            putU16(txbuf, static_cast<uint8_t>(offset + 2U),
                   static_cast<uint16_t>(frequency));
            putU16(txbuf, static_cast<uint8_t>(offset + 4U), state.dutyPermille);
        }
    }

    void writeSentResponse(uint8_t *txbuf, uint8_t command, uint8_t input)
    {
        if (!validInput(input)) {
            initResponse(txbuf, command, spiResultRange, 0U);
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

        initResponse(txbuf, command, spiResultOk, 9U);
        txbuf[4] = input;
        putU32(txbuf, 5U, frame.data);
        txbuf[9] = frame.status;
        txbuf[10] = frame.crc;
        txbuf[11] = frame.nibbleCount;
        txbuf[12] = (ready ? 1U : 0U) | (frame.valid ? (1U << 1U) : 0U);
    }

    void writeOutputResponse(uint8_t *txbuf, uint8_t command, uint8_t output)
    {
        if (!validOutput(output)) {
            initResponse(txbuf, command, spiResultRange, 0U);
            return;
        }

        const OutputState state = outputGetState(output);

        initResponse(txbuf, command, spiResultOk, 7U);
        txbuf[4] = output;
        putU32(txbuf, 5U, state.frequencyHz);
        putU16(txbuf, 9U, state.duty);
    }

    void buildResponse(const uint8_t *rxbuf, uint8_t *txbuf)
    {
        const uint8_t command = rxbuf[0];

        switch (command) {
        case spiCmdNop:
            initResponse(txbuf, command, spiResultOk, 0U);
            break;
        case spiCmdReadVersion:
            writeVersionResponse(txbuf, command);
            break;
        case spiCmdReadAnalog:
            writeAnalogResponse(txbuf, command);
            break;
        case spiCmdReadDigital:
            writeDigitalResponse(txbuf, command, rxbuf[1]);
            break;
        case spiCmdReadDigitalAll:
            writeAllDigitalResponse(txbuf, command);
            break;
        case spiCmdReadSent:
            writeSentResponse(txbuf, command, rxbuf[1]);
            break;
        case spiCmdSetOutput:
            setOutput(txbuf, rxbuf);
            break;
        case spiCmdDisableOutput:
            disableOutput(txbuf, rxbuf);
            break;
        case spiCmdReadOutput:
            writeOutputResponse(txbuf, command, rxbuf[1]);
            break;
        case spiCmdSetInputMode:
            setInputMode(txbuf, rxbuf);
            break;
        case spiCmdEnterUpdate:
            updateRequestedFlag = true;
            initResponse(txbuf, command, spiResultOk, 0U);
            break;
        default:
            initResponse(txbuf, command, spiResultInvalid, 0U);
            break;
        }
    }

    void setOutput(uint8_t *txbuf, const uint8_t *rxbuf)
    {
        if (outputSet(rxbuf[1], getU32(rxbuf, 2U), getU16(rxbuf, 6U))) {
            writeOutputResponse(txbuf, rxbuf[0], rxbuf[1]);
        }
        else {
            initResponse(txbuf, rxbuf[0], spiResultRange, 0U);
        }
    }

    void disableOutput(uint8_t *txbuf, const uint8_t *rxbuf)
    {
        if (validOutput(rxbuf[1])) {
            outputDisable(rxbuf[1]);
            writeOutputResponse(txbuf, rxbuf[0], rxbuf[1]);
        }
        else {
            initResponse(txbuf, rxbuf[0], spiResultRange, 0U);
        }
    }

    void setInputMode(uint8_t *txbuf, const uint8_t *rxbuf)
    {
        if (inputSetMode(rxbuf[1], static_cast<DigitalInputMode>(rxbuf[2]))) {
            writeDigitalResponse(txbuf, rxbuf[0], rxbuf[1]);
        }
        else {
            initResponse(txbuf, rxbuf[0], spiResultRange, 0U);
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
