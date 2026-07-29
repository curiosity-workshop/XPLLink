#pragma once

#include <xpllink/transport/IByteTransport.h>

#include <cstdint>
#include <string>

namespace xpllink::serial
{
    enum class MacSerialControlMode
    {
        DtrRtsDisabled,
        DtrRtsEnabled
    };

    class MacSerialTransport final
        : public transport::IByteTransport
    {
    public:
        explicit MacSerialTransport(
            std::string portName,
            std::uint32_t baudRate = 115200,
            MacSerialControlMode controlMode =
                MacSerialControlMode::DtrRtsDisabled);

        ~MacSerialTransport() override;

        MacSerialTransport(
            const MacSerialTransport&) = delete;

        MacSerialTransport& operator=(
            const MacSerialTransport&) = delete;

        bool open() override;
        void close() override;
        bool isOpen() const override;

        std::size_t read(
            std::span<std::byte> buffer) override;

        std::size_t write(
            std::span<const std::byte> data) override;

        const std::string& portName() const;

    private:
        bool configurePort();
        void applyControlMode();

        std::string portName_;
        std::uint32_t baudRate_;
        MacSerialControlMode controlMode_;
        int descriptor_ = -1;
    };
}
