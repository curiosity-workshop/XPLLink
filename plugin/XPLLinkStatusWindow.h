#pragma once

#include <XPLMDisplay.h>

#include <functional>
#include <string>
#include <vector>

namespace xpllink::plugin
{
    class XPLLinkStatusWindow
    {
    public:
        using StatusProvider =
            std::function<std::vector<std::string>()>;

        explicit XPLLinkStatusWindow(
            StatusProvider statusProvider);
        ~XPLLinkStatusWindow();

        XPLLinkStatusWindow(const XPLLinkStatusWindow&) = delete;
        XPLLinkStatusWindow& operator=(const XPLLinkStatusWindow&) = delete;

        void toggle();
        void open();
        void close();

        [[nodiscard]] bool isOpen() const;

    private:
        static void drawCallback(
            XPLMWindowID window,
            void* refcon);

        static void keyCallback(
            XPLMWindowID window,
            char key,
            XPLMKeyFlags flags,
            char virtualKey,
            void* refcon,
            int losingFocus);

        static int mouseClickCallback(
            XPLMWindowID window,
            int x,
            int y,
            XPLMMouseStatus mouse,
            void* refcon);

        static XPLMCursorStatus cursorCallback(
            XPLMWindowID window,
            int x,
            int y,
            void* refcon);

        static int mouseWheelCallback(
            XPLMWindowID window,
            int x,
            int y,
            int wheel,
            int clicks,
            void* refcon);

        void draw(
            XPLMWindowID window) const;

        StatusProvider statusProvider_;
        XPLMWindowID window_ = nullptr;
    };
}
