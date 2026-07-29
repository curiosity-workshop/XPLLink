#pragma once

#include <xpllink/logging/Log.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

namespace xpllink::logging
{
    class ISerialTraceSink
    {
    public:
        virtual ~ISerialTraceSink() = default;

        virtual void bytes(
            ByteDumpDirection direction,
            std::string_view portName,
            std::span<const std::byte> data) = 0;
    };

    class SerialTraceLogger final : public ISerialTraceSink
    {
    public:
        explicit SerialTraceLogger(
            const std::filesystem::path& path);

        bool isOpen() const;

        void bytes(
            ByteDumpDirection direction,
            std::string_view portName,
            std::span<const std::byte> data) override;

    private:
        std::ofstream stream_;
    };
}
