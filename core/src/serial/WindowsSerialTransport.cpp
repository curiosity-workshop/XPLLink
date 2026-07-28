#include <phoenix/serial/WindowsSerialTransport.h>
#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace phoenix::serial
{
    namespace
    {
        std::string makeWindowsPortPath(
            const std::string& portName)
        {
            if (portName.starts_with(R"(\\.\)"))
            {
                return portName;
            }

            return R"(\\.\)" + portName;
        }
    }

    WindowsSerialTransport::WindowsSerialTransport(
        std::string portName,
        std::uint32_t baudRate,
        WindowsSerialControlMode controlMode)
        : portName_(std::move(portName)),
        baudRate_(baudRate),
        controlMode_(controlMode)
    {
    }

    WindowsSerialTransport::~WindowsSerialTransport()
    {
        close();
    }

    bool WindowsSerialTransport::open()
    {
        if (isOpen())
        {
            return true;
        }

        clearLastError();

        const std::string devicePath =
            makeWindowsPortPath(portName_);

        handle_ = CreateFileA(
            devicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (handle_ == INVALID_HANDLE_VALUE)
        {
            captureLastError(
                "CreateFileA");
            return false;
        }

        if (!configurePort())
        {
            close();
            return false;
        }

        PurgeComm(
            handle_,
            PURGE_RXABORT |
            PURGE_RXCLEAR |
            PURGE_TXABORT |
            PURGE_TXCLEAR);

        return true;
    }

    bool WindowsSerialTransport::configurePort()
    {
        DCB configuration{};
        configuration.DCBlength = sizeof(configuration);

        if (!GetCommState(handle_, &configuration))
        {
            captureLastError(
                "GetCommState");
            return false;
        }

        configuration.BaudRate = baudRate_;
        configuration.ByteSize = 8;
        configuration.StopBits = ONESTOPBIT;
        configuration.Parity = NOPARITY;

        configuration.fBinary = TRUE;
        configuration.fParity = FALSE;

        configuration.fOutxCtsFlow = FALSE;
        configuration.fOutxDsrFlow = FALSE;
        configuration.fDsrSensitivity = FALSE;

        configuration.fOutX = FALSE;
        configuration.fInX = FALSE;

        if (controlMode_ == WindowsSerialControlMode::DtrRtsEnabled)
        {
            configuration.fDtrControl = DTR_CONTROL_ENABLE;
            configuration.fRtsControl = RTS_CONTROL_ENABLE;
        }
        else
        {
            configuration.fDtrControl = DTR_CONTROL_DISABLE;
            configuration.fRtsControl = RTS_CONTROL_DISABLE;
        }

        configuration.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &configuration))
        {
            captureLastError(
                "SetCommState");
            return false;
        }

        COMMTIMEOUTS timeouts{};

        // Return immediately with whatever bytes are currently available.
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;

        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 500;

        if (!SetCommTimeouts(handle_, &timeouts))
        {
            captureLastError(
                "SetCommTimeouts");
            return false;
        }

        return true;
    }

    void WindowsSerialTransport::close()
    {
        if (!isOpen())
        {
            return;
        }

        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }

    bool WindowsSerialTransport::isOpen() const
    {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    std::size_t WindowsSerialTransport::read(
        std::span<std::byte> buffer)
    {
        if (!isOpen() || buffer.empty())
        {
            return 0;
        }

        const DWORD requested =
            static_cast<DWORD>(
                std::min<std::size_t>(
                    buffer.size(),
                    std::numeric_limits<DWORD>::max()));

        DWORD bytesRead = 0;

        if (!ReadFile(
            handle_,
            buffer.data(),
            requested,
            &bytesRead,
            nullptr))
        {
            captureLastError(
                "ReadFile");
            return 0;
        }

        return static_cast<std::size_t>(bytesRead);
    }

    std::size_t WindowsSerialTransport::write(
        std::span<const std::byte> data)
    {
        if (!isOpen() || data.empty())
        {
            return 0;
        }

        const DWORD requested =
            static_cast<DWORD>(
                std::min<std::size_t>(
                    data.size(),
                    std::numeric_limits<DWORD>::max()));

        DWORD bytesWritten = 0;

        if (!WriteFile(
            handle_,
            data.data(),
            requested,
            &bytesWritten,
            nullptr))
        {
            captureLastError(
                "WriteFile");
            return 0;
        }

        return static_cast<std::size_t>(bytesWritten);
    }

    const std::string&
        WindowsSerialTransport::portName() const
    {
        return portName_;
    }

    DWORD WindowsSerialTransport::lastErrorCode() const
    {
        return lastErrorCode_;
    }

    const std::string& WindowsSerialTransport::lastErrorMessage() const
    {
        return lastErrorMessage_;
    }

    void WindowsSerialTransport::captureLastError(
        const char* operation)
    {
        lastErrorCode_ =
            GetLastError();

        std::ostringstream message;
        message
            << operation
            << " failed";

        if (lastErrorCode_ != ERROR_SUCCESS)
        {
            message
                << " with Win32 error "
                << lastErrorCode_;
        }

        lastErrorMessage_ =
            message.str();
    }

    void WindowsSerialTransport::clearLastError()
    {
        lastErrorCode_ =
            ERROR_SUCCESS;
        lastErrorMessage_.clear();
    }
}
