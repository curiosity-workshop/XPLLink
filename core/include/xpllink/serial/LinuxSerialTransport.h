#pragma once

#include <xpllink/transport/IByteTransport.h>

#include <cstdint>
#include <string>

namespace xpllink::serial
{
    enum class LinuxSerialControlMode
    {
        DtrRtsDisabled,
        DtrRtsEnabled
    };

    class LinuxSerialTransport final
        : public transport::IByteTransport
    {
    public:
        explicit LinuxSerialTransport(
            std::string portName,
            std::uint32_t baudRate = 115200,
            LinuxSerialControlMode controlMode =
                LinuxSerialControlMode::DtrRtsDisabled);

        ~LinuxSerialTransport() override;

        LinuxSerialTransport(
            const LinuxSerialTransport&) = delete;

        LinuxSerialTransport& operator=(
            const LinuxSerialTransport&) = delete;

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
        LinuxSerialControlMode controlMode_;
        int descriptor_ = -1;
    };
}
