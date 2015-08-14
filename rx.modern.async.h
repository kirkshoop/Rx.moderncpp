#pragma once

#include <rx.modern.h>
namespace Rx {
using namespace Modern;

inline void print_error(std::exception_ptr ep) {
    try { std::rethrow_exception(ep); }
    catch (const Modern::Exception& ex) { printf("\nerror! 0x%x\n", ex.Result); }
    catch (const std::exception& ex) { printf("\nerror! %s\n", ex.what()); }
}

template<class Result>
auto from_async(const Windows::Foundation::IAsyncOperation<Result>& aop) -> Rx::observable<Result> {
    return Rx::create<Result>(
        [=](Rx::subscriber<Result>& out) {
            aop.Completed([=](Windows::Foundation::IAsyncOperation<Result> const & completed, AsyncStatus const &) {
                try { out.on_next(completed.GetResults()); }
                catch (...) { out.on_error(std::current_exception()); }
                out.on_completed();
            });
            // no cancellation :(
        })
        .as_dynamic()
        .publish()
        .ref_count();
}

template<class Result, class Progress>
auto from_async(const Windows::Foundation::IAsyncOperationWithProgress<Result, Progress>& aop) -> Rx::observable<Result> {
    return Rx::create<Result>(
        [=](Rx::subscriber<Result>& out) {
            aop.Completed([=](Windows::Foundation::IAsyncOperationWithProgress<Result, Progress> const & completed, AsyncStatus const &) {
                try { out.on_next(completed.GetResults()); }
                catch (...) { out.on_error(std::current_exception()); }
                out.on_completed();
            });
            // no cancellation :(
        })
        .as_dynamic()
        .publish()
        .ref_count();
}

template<class Async>
auto start_async(Async&& a) -> decltype(from_async(a())) {
    return Rx::defer(
        [=]() {
            return from_async(a());
        });
}

namespace detail {

template<class Result, class Progress>
struct r_and_p 
{
    Rx::maybe<Result> r;
    Rx::maybe<Progress> p;
    Rx::subject<Result> rsub;
    Rx::subject<Progress> psub;
};

}

template<class Result, class Progress>
auto from_async_with_progress(const Windows::Foundation::IAsyncOperationWithProgress<Result, Progress>& aop) -> std::tuple<Rx::observable<Result>, Rx::observable<Progress>> {
    auto state = make_shared < detail::r_and_p<Result, Progress> >();

    aop.Progress([=](Windows::Foundation::IAsyncOperationWithProgress<Result, Progress> const &, Progress const & progress) {
        auto ps = state->psub.get_subscriber();
        state->p.reset(progress);
        ps.on_next(progress);
    });

    aop.Completed([=](Windows::Foundation::IAsyncOperationWithProgress<Result, Progress> const & completed, AsyncStatus const &) {
        auto rs = state->rsub.get_subscriber();
        auto ps = state->psub.get_subscriber();
        try { state->r.reset(completed.GetResults()); }
        catch (...) { rs.on_error(std::current_exception()); }
        rs.on_next(state->r.get());
        rs.on_completed();
        ps.on_completed();
    });

    auto result = Rx::defer(
        [=]() -> Rx::observable<Result> {
            auto result = state->rsub
                .get_observable();
            if (!state->r.empty()) {
                return result
                    .start_with(state->r.get());
            }
            return result;
        })
        .as_dynamic();

    auto progress = Rx::defer(
        [=]() -> Rx::observable<Progress> {
            auto result = state->psub
                .get_observable();
            if (!state->p.empty()) {
                return result
                    .start_with(state->p.get());
            }
            return result;
        })
        .as_dynamic();

    return std::make_tuple(result, progress);
}

template<class Async, 
    class AOP = std::result_of_t<Async()>, 
    class Tuple = decltype(from_async_with_progress(AOP())), 
    class Result = Rx::observable<std::decay_t<Tuple>>>
auto start_async_with_progress(Async&& a) -> Result {
    return Rx::create<Tuple>(
        [=](Rx::subscriber<Tuple>& out) {
            out.on_next(from_async_with_progress(a()));
            out.on_completed();
        });
}

template<template<class> class Items, class Item>
auto to_vector(Items<Item> items) {
    std::vector<Item> copy;
    for (Item const & item : items) {
        copy.push_back(item);
    }
    return copy;
}

}