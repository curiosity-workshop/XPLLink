#include <xpllink/serial/MacSerialTransport.h>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace xpllink::serial
{
    namespace
    {
        speed_t toSpeed(
            std::uint32_t baudRate)
        {
            switch (baudRate)
            {
            case 9600:
                return B9600;
            case 19200:
                return B19200;
            case 38400:
                return B38400;
            case 57600:
                return B57600;
            case 115200:
                return B115200;
            case 230400:
#ifdef B230400
                return B230400;
#else
                return B115200;
#endif
            default:
                return B115200;
            }
        }
    }

    MacSerialTransport::MacSerialTransport(
        std::string portName,
        std::uint32_t baudRate,
        MacSerialControlMode controlMode)
        : portName_(std::move(portName)),
        baudRate_(baudRate),
        controlMode_(controlMode)
    {
    }

    MacSerialTransport::~MacSerialTransport()
    {
        close();
    }

    bool MacSerialTransport::open()
    {
        if (isOpen())
        {
            return true;
        }

        descriptor_ = ::open(
            portName_.c_str(),
            O_RDWR | O_NOCTTY | O_NONBLOCK);

        if (descriptor_ < 0)
        {
            return false;
        }

        if (!configurePort())
        {
            close();
            return false;
        }

        ::tcflush(descriptor_, TCIOFLUSH);
        return true;
    }

    bool MacSerialTransport::configurePort()
    {
        termios configuration{};

        if (::tcgetattr(descriptor_, &configuration) != 0)
        {
            return false;
        }

        ::cfmakeraw(&configuration);
        configuration.c_cflag |= CLOCAL | CREAD;
#ifdef CRTSCTS
        configuration.c_cflag &= ~CRTSCTS;
#endif
        configuration.c_cflag &= ~PARENB;
        configuration.c_cflag &= ~CSTOPB;
        configuration.c_cflag &= ~CSIZE;
        configuration.c_cflag |= CS8;
        configuration.c_cc[VMIN] = 0;
        configuration.c_cc[VTIME] = 0;

        const speed_t speed = toSpeed(baudRate_);

        if (::cfsetspeed(&configuration, speed) != 0)
        {
            return false;
        }

        if (::tcsetattr(descriptor_, TCSANOW, &configuration) != 0)
        {
            return false;
        }

        applyControlMode();
        return true;
    }

    void MacSerialTransport::applyControlMode()
    {
        int lines = TIOCM_DTR | TIOCM_RTS;

        if (controlMode_ == MacSerialControlMode::DtrRtsEnabled)
        {
            ::ioctl(descriptor_, TIOCMBIS, &lines);
        }
        else
        {
            ::ioctl(descriptor_, TIOCMBIC, &lines);
        }
    }

    void MacSerialTransport::close()
    {
        if (!isOpen())
        {
            return;
        }

        ::close(descriptor_);
        descriptor_ = -1;
    }

    bool MacSerialTransport::isOpen() const
    {
        return descriptor_ >= 0;
    }

    std::size_t MacSerialTransport::read(
        std::span<std::byte> buffer)
    {
        if (!isOpen() || buffer.empty())
        {
            return 0;
        }

        const std::size_t requested =
            std::min<std::size_t>(
                buffer.size(),
                static_cast<std::size_t>(
                    std::numeric_limits<ssize_t>::max()));

        const ssize_t bytesRead =
            ::read(
                descriptor_,
                buffer.data(),
                requested);

        if (bytesRead <= 0)
        {
            return 0;
        }

        return static_cast<std::size_t>(bytesRead);
    }

    std::size_t MacSerialTransport::write(
        std::span<const std::byte> data)
    {
        if (!isOpen() || data.empty())
        {
            return 0;
        }

        const std::size_t requested =
            std::min<std::size_t>(
                data.size(),
                static_cast<std::size_t>(
                    std::numeric_limits<ssize_t>::max()));

        const ssize_t bytesWritten =
            ::write(
                descriptor_,
                data.data(),
                requested);

        if (bytesWritten <= 0)
        {
            return 0;
        }

        return static_cast<std::size_t>(bytesWritten);
    }

    const std::string&
        MacSerialTransport::portName() const
    {
        return portName_;
    }
}
