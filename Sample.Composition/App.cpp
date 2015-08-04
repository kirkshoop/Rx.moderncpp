#include "pch.h"

using namespace Modern;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::System;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

static float const BlockSize = 100.0f;

Point PositionOf(PointerEventArgs const & args) {
    return args.CurrentPoint().Position();
}

struct View : IFrameworkViewT<View>
{
    CompositionTarget m_target = nullptr;

    void SetWindow(CoreWindow const & window)
    {
        Compositor compositor;
        ContainerVisual root = compositor.CreateContainerVisual();
        m_target = compositor.CreateTargetForCurrentView();
        m_target.Root(root);

        // NOTE: moderncpp may soon include overloads for each
        // event that will allow:
        //   auto pressed = windows.PointerPressed();

        auto pressed = Rx::from_event<PointerEventArgs>(
            [=](auto h) {
                return window.PointerPressed(h);
            },
            [=](auto t) {
                window.PointerPressed(t);
            });

        auto moved = Rx::from_event<PointerEventArgs>(
            [=](auto h) {
                return window.PointerMoved(h);
            },
            [=](auto t) {
                window.PointerMoved(t);
            });

        auto released = Rx::from_event<PointerEventArgs>(
            [=](auto h) {
                return window.PointerReleased(h);
            },
            [=](auto t) {
                window.PointerReleased(t);
            });

        auto visuals = Rx::observable<>::from(root.Children());

        // get the position for a press when ctrl is pressed.
        auto adds = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() == VirtualKeyModifiers::Control; 
            })
            .map(&PositionOf);

        // get the position for a press when ctrl is NOT pressed.
        auto selects = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() != VirtualKeyModifiers::Control; 
            })
            .map(&PositionOf);

        // adds visuals
        adds
            .combine_latest(
                [](Point const & position, VisualCollection visuals) {
                    View::AddVisual(visuals, position);
                    return visuals.Count();
                }, 
                visuals)
            .subscribe();

        // moves selected visuals
        selects
            .combine_latest(
                [](Point const & position, VisualCollection visuals) {
                    Visual selected = View::SelectVisual(visuals, position);
                    return std::make_tuple(position, selected);
                },
                visuals)
            .filter(Rx::apply_to([](Point const &, Visual const & selected) {return !!selected; }))
            .map(Rx::apply_to([=](Point const & position, Visual const & selected) {

                Vector3 window_offset = selected.Offset();
                Vector3 mouse_offset = {
                    window_offset.X - position.X,
                    window_offset.Y - position.Y
                };

                // moved and released events are only consumed while the pointer 
                // is pressed.
                // when idle, the view is only listening to presssed events.
                // NOTE: this is why from_event needs to know how to remove.
                return moved
                    .map(&PositionOf)
                    .map([=](Point const & position) { 
                        return std::make_tuple(selected, mouse_offset, position); 
                    })
                    .take_until(released);
            }))
            .merge()
            .subscribe(Rx::apply_to([](Visual selected, Vector3 offset, Point position) {
                selected.Offset(Vector3
                {
                    position.X + offset.X,
                    position.Y + offset.Y
                });
            }));
    }

    static void AddVisual(VisualCollection& visuals, Point point)
    {
        Compositor compositor = visuals.Compositor();
        SolidColorVisual visual = compositor.CreateSolidColorVisual();

        static Color colors[] =
        {
            { 0xDC, 0x5B, 0x9B, 0xD5 },
            { 0xDC, 0xED, 0x7D, 0x31 },
            { 0xDC, 0x70, 0xAD, 0x47 },
            { 0xDC, 0xFF, 0xC0, 0x00 }
        };

        static unsigned last = 0;
        unsigned next = ++last % _countof(colors);
        visual.Color(colors[next]);

        visual.Size(Vector2
        {
            BlockSize,
            BlockSize
        });

        visual.Offset(Vector3
        {
            point.X - BlockSize / 2.0f,
            point.Y - BlockSize / 2.0f,
        });

        visuals.InsertAtTop(visual);
    }

    static Visual SelectVisual(VisualCollection& visuals, Point point)
    {
        Visual selected = nullptr;
        for (Visual const & visual : visuals)
        {
            Vector3 offset = visual.Offset();
            Vector2 size = visual.Size();

            if (point.X >= offset.X &&
                point.X < offset.X + size.X &&
                point.Y >= offset.Y &&
                point.Y < offset.Y + size.Y)
            {
                selected = visual;
            }
        }
        if (selected) {
            visuals.Remove(selected);
            visuals.InsertAtTop(selected);
        }
        return selected;
    }
};

struct ViewSource : IFrameworkViewSourceT<ViewSource>
{
    IFrameworkView CreateView()
    {
        return make<View>();
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<ViewSource>());
}