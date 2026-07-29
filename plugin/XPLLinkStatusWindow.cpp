#include "XPLLinkStatusWindow.h"

#include <XPLMGraphics.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace xpllink::plugin
{
    namespace
    {
        constexpr int WindowWidth = 780;
        constexpr int WindowHeight = 230;
        constexpr int Margin = 10;
        constexpr int LineHeight = 15;

        void drawLine(
            int left,
            int top,
            int lineIndex,
            const std::string& text,
            float* color)
        {
            XPLMDrawString(
                color,
                left + Margin,
                top - Margin - ((lineIndex + 1) * LineHeight),
                text.c_str(),
                nullptr,
                xplmFont_Basic);
        }
    }

    XPLLinkStatusWindow::XPLLinkStatusWindow(
        StatusProvider statusProvider)
        : statusProvider_(std::move(statusProvider))
    {
    }

    XPLLinkStatusWindow::~XPLLinkStatusWindow()
    {
        close();
    }

    void XPLLinkStatusWindow::toggle()
    {
        if (isOpen())
        {
            close();
            return;
        }

        open();
    }

    void XPLLinkStatusWindow::open()
    {
        if (isOpen())
        {
            return;
        }

        int screenLeft = 0;
        int screenTop = 0;
        int screenRight = 0;
        int screenBottom = 0;
        XPLMGetScreenBoundsGlobal(
            &screenLeft,
            &screenTop,
            &screenRight,
            &screenBottom);

        const int left =
            screenLeft + 50;
        const int top =
            screenTop - 60;
        const int right =
            (std::min)(left + WindowWidth, screenRight - 20);
        const int bottom =
            (std::max)(top - WindowHeight, screenBottom + 20);

        XPLMCreateWindow_t params{};
        params.structSize =
            sizeof(params);
        params.left =
            left;
        params.top =
            top;
        params.right =
            right;
        params.bottom =
            bottom;
        params.visible =
            1;
        params.drawWindowFunc =
            drawCallback;
        params.handleMouseClickFunc =
            mouseClickCallback;
        params.handleKeyFunc =
            keyCallback;
        params.handleCursorFunc =
            cursorCallback;
        params.handleMouseWheelFunc =
            mouseWheelCallback;
        params.refcon =
            this;
        params.decorateAsFloatingWindow =
            xplm_WindowDecorationSelfDecoratedResizable;
        params.layer =
            xplm_WindowLayerFloatingWindows;
        params.handleRightClickFunc =
            mouseClickCallback;

        window_ =
            XPLMCreateWindowEx(&params);
    }

    void XPLLinkStatusWindow::close()
    {
        if (!isOpen())
        {
            return;
        }

        XPLMDestroyWindow(window_);
        window_ =
            nullptr;
    }

    bool XPLLinkStatusWindow::isOpen() const
    {
        return window_ != nullptr;
    }

    void XPLLinkStatusWindow::drawCallback(
        XPLMWindowID window,
        void* refcon)
    {
        auto* statusWindow =
            static_cast<XPLLinkStatusWindow*>(refcon);

        if (statusWindow != nullptr)
        {
            statusWindow->draw(window);
        }
    }

    void XPLLinkStatusWindow::keyCallback(
        XPLMWindowID,
        char,
        XPLMKeyFlags,
        char,
        void*,
        int)
    {
    }

    int XPLLinkStatusWindow::mouseClickCallback(
        XPLMWindowID,
        int,
        int,
        XPLMMouseStatus mouse,
        void* refcon)
    {
        auto* statusWindow =
            static_cast<XPLLinkStatusWindow*>(refcon);

        if (statusWindow != nullptr &&
            mouse == xplm_MouseUp)
        {
            statusWindow->close();
        }

        return 1;
    }

    XPLMCursorStatus XPLLinkStatusWindow::cursorCallback(
        XPLMWindowID,
        int,
        int,
        void*)
    {
        return xplm_CursorDefault;
    }

    int XPLLinkStatusWindow::mouseWheelCallback(
        XPLMWindowID,
        int,
        int,
        int,
        int,
        void*)
    {
        return 1;
    }

    void XPLLinkStatusWindow::draw(
        XPLMWindowID window) const
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        XPLMGetWindowGeometry(
            window,
            &left,
            &top,
            &right,
            &bottom);

        XPLMDrawTranslucentDarkBox(
            left,
            top,
            right,
            bottom);

        std::array<float, 3> white{
            1.0f,
            1.0f,
            1.0f
        };

        std::vector<std::string> lines;
        lines.emplace_back(
            "Curiosity Workshop XPLLink Interface");
        lines.emplace_back(
            std::string{ "Distribution Build: " } +
            __DATE__ +
            " " +
            __TIME__ +
            ". Click this window when complete");

        if (statusProvider_)
        {
            const auto providedLines =
                statusProvider_();
            lines.insert(
                lines.end(),
                providedLines.begin(),
                providedLines.end());
        }

        const int maxLines =
            (std::max)(0, ((top - bottom) - (2 * Margin)) / LineHeight);

        for (int index = 0;
            index < static_cast<int>(lines.size()) &&
            index < maxLines;
            ++index)
        {
            drawLine(
                left,
                top,
                index,
                lines[static_cast<std::size_t>(index)],
                white.data());
        }
    }
}
