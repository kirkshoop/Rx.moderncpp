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

// I would really like to keep Visual and PointerEventArgs in a tuple - Please?
struct move_selected_t
{
    move_selected_t() : selected(nullptr), offset(), args(nullptr) {}
    move_selected_t(Visual s, Vector3 o, PointerEventArgs a) : selected(s), offset(o), args(a) {}
    Visual selected;
    Vector3 offset;
    PointerEventArgs args;
};

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

        auto pressed = Rx::from_event<PointerEventArgs>([=](auto h){
                return window.PointerPressed(h);
            },
            [=](auto t) {
                window.PointerPressed(t);
            });

        auto moved = Rx::from_event<PointerEventArgs>([=](auto h) {
                return window.PointerMoved(h);
            },
            [=](auto t) {
                window.PointerMoved(t);
            });

        auto released = Rx::from_event<PointerEventArgs>([=](auto h) {
                return window.PointerReleased(h);
            },
            [=](auto t) {
                window.PointerReleased(t);
            });

        auto adds = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() == VirtualKeyModifiers::Control; 
            })
            .as_dynamic();

        auto selects = pressed
            .filter([](PointerEventArgs const & args) {
                return args.KeyModifiers() != VirtualKeyModifiers::Control; 
            })
            .as_dynamic();

        adds
            .subscribe([this](PointerEventArgs const & args) {
                Point point = args.CurrentPoint().Position();
                AddVisual(point);
            });

        selects
            .map([=](PointerEventArgs const & args) {
                Point point = args.CurrentPoint().Position();
                Visual selected = SelectVisual(point);
                if (!selected) {
                    return Rx::observable<>::empty<move_selected_t>().as_dynamic();
                }
                Vector3 window_offset = selected.Offset();
                Vector3 mouse_offset = {
                    window_offset.X - point.X,
                    window_offset.Y - point.Y
                };
                // moved and released events are only consumed while the pointer is pressed
                // at idle only presssed events are still being listened to.
                // this is why from_event needs to know how to remove and add.
                return moved
                    .map([=](PointerEventArgs const & args) { return move_selected_t(selected, mouse_offset, args); })
                    .take_until(released)
                    .as_dynamic();
            })
            .merge()
            .subscribe([](move_selected_t mt) {
                Point point = mt.args.CurrentPoint().Position();

                mt.selected.Offset(Vector3
                {
                    point.X + mt.offset.X,
                    point.Y + mt.offset.Y
                });
            });
	}

	void ScaleVisuals()
	{
		for (Visual const & visual : m_visuals)
		{
			Vector3 offset = visual.Offset();
			Vector2 size = visual.Size();

			visual.Offset(Vector3
			{
				offset.X + size.X / 2.0f - BlockSize / 2.0f,
				offset.Y + size.Y / 2.0f - BlockSize / 2.0f,
			});

			visual.Size(Vector2
			{
				BlockSize,
				BlockSize,
			});
		}
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