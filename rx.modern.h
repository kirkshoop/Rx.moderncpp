#include <rx.hpp>
namespace Rx {
    using namespace rxcpp;
    using namespace rxcpp::sources;
    using namespace rxcpp::operators;
    using namespace rxcpp::subjects;
    using namespace rxcpp::util;
}
namespace Rx {
    template<class Event, class Add, class Remove>
    auto from_event(Add&& a, Remove&& r) {
        return create<PointerEventArgs>(
            [=](subscriber<PointerEventArgs> out) {
                auto token = a([=](auto const &, PointerEventArgs const & args) {
                    out.on_next(args);
                });
                out.add([=]() {r(token); });
            })
            .publish()
            .ref_count()
            .as_dynamic();
    }
}
