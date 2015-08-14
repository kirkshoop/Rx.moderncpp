#pragma once

namespace Rx {

    namespace wf = Windows::Foundation;
    namespace wuicore = Windows::UI::Core;
    namespace wuixaml = Windows::UI::Xaml;
    namespace wthread = Windows::System::Threading;

    struct core_dispatcher : public scheduler_interface
    {
    private:
        typedef core_dispatcher this_type;
        core_dispatcher(const this_type&);

        struct core_dispatcher_worker : public worker_interface
        {
        private:
            typedef core_dispatcher_worker this_type;
            core_dispatcher_worker(const this_type&);

            wuicore::CoreDispatcher dispatcher;
            wuicore::CoreDispatcherPriority priority;
        public:
            virtual ~core_dispatcher_worker()
            {
            }
            core_dispatcher_worker(wuicore::CoreDispatcher dispatcher, wuicore::CoreDispatcherPriority priority)
                : dispatcher(dispatcher)
                , priority(priority)
            {
            }

            virtual clock_type::time_point now() const {
                return clock_type::now();
            }

            virtual void schedule(const schedulable& scbl) const {
                dispatcher.RunAsync(
                    priority, 
                    [scbl]() {
                    if (scbl.is_subscribed()) {
                        // disallow recursion
                        recursion r(false);
                        scbl(r.get_recurse());
                    }
                });
            }

            virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
                auto that = shared_from_this();

                auto now = this->now();
                auto interval = when - now;
                if (now > when || interval < std::chrono::milliseconds(10))
                {
                    schedule(scbl);
                    return;
                }

                wf::TimeSpan timeSpan;

                // convert to 100ns ticks
                timeSpan.Duration = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count() / 100);

                auto dispatcherTimer = wuixaml::DispatcherTimer();

                dispatcherTimer.Interval(timeSpan);

                auto token = dispatcherTimer.Tick(
                    [dispatcherTimer, that, this, scbl](Windows::IInspectable, Windows::IInspectable) mutable {
                        dispatcherTimer.Stop();
                        schedule(scbl);
                    });

                scbl.add(
                    [dispatcherTimer, token]() {
                        dispatcherTimer.Tick(token);
                    });

                // don't turn S_FALSE into exception
                dispatcherTimer->abi_Start();
            }
        };

        std::shared_ptr<core_dispatcher_worker> wi;

    public:
        core_dispatcher(wuicore::CoreDispatcher dispatcher, wuicore::CoreDispatcherPriority priority)
            : wi(std::make_shared<core_dispatcher_worker>(dispatcher, priority))
        {
        }
        virtual ~core_dispatcher()
        {
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual worker create_worker(composite_subscription cs) const {
            return worker(std::move(cs), wi);
        }
    };

    inline scheduler make_core_dispatcher(wuicore::CoreDispatcher dispatcher, wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        scheduler instance = make_scheduler<core_dispatcher>(dispatcher, priority);
        return instance;
    }

    inline scheduler make_core_dispatcher(wuixaml::Window window, wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        auto d = window.Dispatcher();
        if (d == nullptr)
        {
            throw std::logic_error("No dispatcher on current window");
        }
        return make_core_dispatcher(d, priority);
    }

    inline scheduler make_core_dispatcher(wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        auto window = wuixaml::Window::Current();
        if (window == nullptr)
        {
            throw std::logic_error("No window current");
        }
        return make_core_dispatcher(window, priority);
    }

    inline identity_one_worker identity_core_dispatcher(wuicore::CoreDispatcher dispatcher, wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        identity_one_worker r(make_core_dispatcher(dispatcher, priority));
        return r;
    }
    inline identity_one_worker identity_core_dispatcher(wuixaml::Window window, wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        identity_one_worker r(make_core_dispatcher(window, priority));
        return r;
    }
    inline identity_one_worker identity_core_dispatcher(wuicore::CoreDispatcherPriority priority = wuicore::CoreDispatcherPriority::Normal) {
        identity_one_worker r(make_core_dispatcher(priority));
        return r;
    }


    struct thread_pool : public scheduler_interface
    {
    private:
        typedef thread_pool this_type;
        thread_pool(const this_type&);

        struct thread_pool_worker : public worker_interface
        {
        private:
            typedef thread_pool_worker this_type;
            thread_pool_worker(const this_type&);

            wthread::WorkItemPriority priority;

        public:
            virtual ~thread_pool_worker()
            {
            }
            explicit thread_pool_worker(wthread::WorkItemPriority priority = wthread::WorkItemPriority::Normal)
                : priority(priority)
            {
            }

            virtual clock_type::time_point now() const {
                return clock_type::now();
            }

            virtual void schedule(const schedulable& scbl) const {
                wthread::ThreadPool::RunAsync([scbl](wf::IAsyncAction) {
                    if (scbl.is_subscribed()) {
                        // allow recursion
                        recursion r(true);
                        scbl(r.get_recurse());
                    }
                }, priority);
            }

            virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
                auto now = this->now();
                auto interval = when - now;
                if (now > when || interval < std::chrono::milliseconds(10))
                {
                    interval = std::chrono::milliseconds(1);
                }

                wf::TimeSpan timeSpan;

                // convert to 100ns ticks
                timeSpan.Duration = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count() / 100);

                auto timer = wthread::ThreadPoolTimer::CreateTimer(
                    [scbl](wthread::ThreadPoolTimer) mutable {
                        if (scbl.is_subscribed()) {
                            // allow recursion
                            recursion r(true);
                            scbl(r.get_recurse());
                        }
                    },
                    timeSpan);

                scbl.add([timer]() {
                    timer.Cancel();
                });
            }
        };

        std::shared_ptr<thread_pool_worker> wi;

    public:
        explicit thread_pool(wthread::WorkItemPriority priority = wthread::WorkItemPriority::Normal)
            : wi(std::make_shared<thread_pool_worker>(priority))
        {
        }
        virtual ~thread_pool()
        {
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual worker create_worker(composite_subscription cs) const {
            return worker(std::move(cs), wi);
        }
    };

    inline scheduler make_thread_pool(wthread::WorkItemPriority priority = wthread::WorkItemPriority::Normal) {
        return make_scheduler<thread_pool>(priority);
    }

    inline serialize_one_worker serialize_thread_pool(wthread::WorkItemPriority priority = wthread::WorkItemPriority::Normal) {
        serialize_one_worker r(make_thread_pool(priority));
        return r;
    }
}