#pragma once 

#include <rx.hpp>
namespace Rx {
    using namespace rxcpp;
    using namespace rxcpp::sources;
    using namespace rxcpp::operators;
    using namespace rxcpp::subjects;
    using namespace rxcpp::util;
    using namespace rxcpp::schedulers;
}
namespace Rx {
    template<class Event, class Add, class Remove>
    observable<Event> from_event(Add&& a, Remove&& r) {
        return create<Event>(
            [=](subscriber<Event> out) {
                auto token = a([=](auto const &, Event const & args) {
                    out.on_next(args);
                });

                out.add([=]() {
                    // this is a destructor - swallow exception from remove
                    try {r(token);}
                    catch (...) {}
                });
            })
            .publish()
            .ref_count();
    }
}


#include <rx.modern.async.h>
#include <rx.modern.schedulers.h>
