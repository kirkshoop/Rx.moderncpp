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
    VisualCollection m_visuals = nullptr;

    void SetWindow(CoreWindow const & window)
    {
        Compositor compositor;
        ContainerVisual root = compositor.CreateContainerVisual();
        m_target = compositor.CreateTargetForCurrentView();
        m_target.Root(root);
        m_visuals = root.Children();

        auto pressed = window.PointerPressed();
        auto moved = window.PointerMoved();
        auto released = window.PointerReleased();

        // get the position for a press when ctrl is pressed.
        auto adds = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() == VirtualKeyModifiers::Control; 
            })
            .map(&PositionOf)
            .as_dynamic();

        // get the position for a press when ctrl is NOT pressed.
        auto selects = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() != VirtualKeyModifiers::Control; 
            })
            .map(&PositionOf)
            .as_dynamic();

        // adds visuals
        adds
            .subscribe(
                [this](Point const & position) {
                    AddVisual(position);
                });

        // moves selected visuals
        selects
            .map([=](Point const & position) {
                using move_selected_t = std::tuple<Rx::maybe<Visual>, Vector3, Point>;

                Visual selected = SelectVisual(position);
                if (!selected) {
                    // have to return an observable of move_selected_t
                    // so return an empty observable
                    return Rx::observable<>::empty<move_selected_t>().as_dynamic();
                }

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
                        return move_selected_t(selected, mouse_offset, position); 
                    })
                    .take_until(released)
                    .as_dynamic();
            })
            .merge()
            .subscribe(
                Rx::apply_to(
                    [](Rx::maybe<Visual> selected, Vector3 offset, Point position) {
                        selected->Offset(Vector3
                        {
                            position.X + offset.X,
                            position.Y + offset.Y
                        });
                    }));
    }

    void AddVisual(Point point)
    {
        Compositor compositor = m_visuals.Compositor();
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

        m_visuals.InsertAtTop(visual);
    }

    Visual SelectVisual(Point point)
    {
        Visual selected = nullptr;
        for (Visual const & visual : m_visuals)
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
            m_visuals.Remove(selected);
            m_visuals.InsertAtTop(selected);
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