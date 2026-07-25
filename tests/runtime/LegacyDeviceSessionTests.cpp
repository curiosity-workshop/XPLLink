#include <phoenix/protocol/legacy/LegacyFrame.h>
#include <phoenix/protocol/legacy/LegacyMessage.h>
#include <phoenix/runtime/LegacyDeviceSession.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class FakeTransport final : public phoenix::transport::IByteTransport
    {
    public:
        bool open() override
        {
            open_ = true;
            return true;
        }

        void close() override
        {
            open_ = false;
        }

        bool isOpen() const override
        {
            return open_;
        }

        std::size_t read(std::span<std::byte> buffer) override
        {
            if (incoming_.empty())
            {
                return 0;
            }

            const auto chunk = incoming_.front();
            incoming_.pop_front();

            const std::size_t bytesToRead =
                std::min(buffer.size(), chunk.size());

            std::copy_n(
                chunk.begin(),
                bytesToRead,
                buffer.begin());

            if (bytesToRead < chunk.size())
            {
                incoming_.push_front(
                    std::vector<std::byte>{
                        chunk.begin() + bytesToRead,
                        chunk.end()
                    });
            }

            return bytesToRead;
        }

        std::size_t write(std::span<const std::byte> data) override
        {
            const std::size_t bytesToWrite =
                maximumWriteSize == 0 ?
                data.size() :
                std::min(maximumWriteSize, data.size());

            written_.insert(
                written_.end(),
                data.begin(),
                data.begin() + bytesToWrite);

            return bytesToWrite;
        }

        void pushIncoming(std::string_view text)
        {
            std::vector<std::byte> bytes;
            bytes.reserve(text.size());

            for (const char character : text)
            {
                bytes.push_back(static_cast<std::byte>(character));
            }

            incoming_.push_back(std::move(bytes));
        }

        std::string writtenText() const
        {
            std::string result;
            result.reserve(written_.size());

            for (const std::byte value : written_)
            {
                result.push_back(
                    static_cast<char>(
                        std::to_integer<unsigned char>(value)));
            }

            return result;
        }

        std::size_t maximumWriteSize = 0;

    private:
        bool open_ = true;
        std::deque<std::vector<std::byte>> incoming_;
        std::vector<std::byte> written_;
    };

    bool expect(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }

        return true;
    }
}

int main()
{
    using phoenix::protocol::legacy::DeviceName;
    using phoenix::runtime::LegacyDeviceSession;
    using phoenix::runtime::LegacyDeviceSessionOptions;

    bool passed = true;

    {
        FakeTransport transport;
        LegacyDeviceSession session{
            transport,
            LegacyDeviceSessionOptions{
                .readBufferSize = 32,
                .maximumReadPasses = 1
            }
        };

        transport.pushIncoming("[n,\"Dev");

        const auto first = session.tick();
        passed &= expect(first.bytesRead == 7, "first tick should read partial frame");
        passed &= expect(first.messages.empty(), "partial frame should not emit message");
        passed &= expect(session.hasPartialInputFrame(), "partial frame should persist");

        transport.pushIncoming("ice\"]");

        const auto second = session.tick();
        passed &= expect(second.messages.size() == 1, "second tick should emit completed frame");

        const auto* name =
            std::get_if<DeviceName>(&second.messages[0]);

        passed &= expect(name != nullptr, "completed frame should decode as device name");
        passed &= expect(name && name->value == "Device", "device name value failed");
        passed &= expect(!session.hasPartialInputFrame(), "completed input should clear parser state");
    }

    {
        FakeTransport transport;
        transport.maximumWriteSize = 2;

        LegacyDeviceSession session{ transport };
        session.queueFrame(
            phoenix::protocol::legacy::sendNameCommand);

        const auto result = session.tick();

        passed &= expect(result.bytesWritten == 3, "session should flush queued frame");
        passed &= expect(transport.writtenText() == "[N]", "queued identity frame bytes failed");
        passed &= expect(!session.hasPendingOutput(), "outgoing queue should be empty");
    }

    {
        FakeTransport transport;
        transport.close();

        LegacyDeviceSession session{ transport };
        session.queueFrame(
            phoenix::protocol::legacy::sendRequestCommand);

        const auto result = session.tick();

        passed &= expect(!result.transportOpen, "closed transport should be reported");
        passed &= expect(result.bytesWritten == 0, "closed transport should not write");
        passed &= expect(session.hasPendingOutput(), "closed transport should retain output");
    }

    return passed ? 0 : 1;
}
