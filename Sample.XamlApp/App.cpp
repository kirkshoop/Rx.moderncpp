#include "pch.h"

using namespace Modern;

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

using namespace std::chrono_literals;

struct App : ApplicationT<App>
{
public:

    void OnLaunched(LaunchActivatedEventArgs const &)
    {
        TextBlock text;

        text.FontFamily(FontFamily(L"Segoe UI Semibold"));
        text.FontSize(48.0);
        text.Foreground(SolidColorBrush(Colors::LightGreen()));
        text.VerticalAlignment(VerticalAlignment::Center);
        text.TextAlignment(TextAlignment::Center);

        Window window = Window::Current();

        window.Content(text);
        window.Activate();

        auto window_coordination = Rx::identity_core_dispatcher(window);

        std::wstring msg(L"Modern C++\nfor the\nWindows Runtime");

        Rx::interval(1s, window_coordination)
            .subscribe([=](long count) {
                std::wstringstream update;
                update << msg << L" (" << count << L")";
                text.Text(update.str().c_str());
            });
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto const &)
    {
        make<App>();
    });
}