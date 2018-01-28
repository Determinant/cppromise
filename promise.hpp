#ifndef _CPPROMISE_HPP
#define _CPPROMISE_HPP

#include <vector>
#include <memory>
#include <any>
#include <functional>

/** Implement type-safe Promise primitives similar to the ones specified by
 * Javascript A+. */
namespace promise {
    using std::function;
    using pm_any_t = std::any;
    using None = std::nullptr_t;
    using values_t = std::vector<pm_any_t>;
    const auto none = nullptr;
    const auto do_nothing = [](){};

    class Promise;
    class promise_t {
        std::shared_ptr<Promise> ptr;
        public:
        promise_t(function<void(promise_t)> callback) {
            ptr = std::make_shared<Promise>();
            callback(*this);
        }

        template<typename T> inline void resolve(T result) const;
        template<typename T> inline void reject(T reason) const;

        template<typename F_, typename F = pm_any_t, typename R_, typename R = pm_any_t>
        inline promise_t then(function<F_(pm_any_t)> fulfilled_callback,
                        function<R_(pm_any_t)> rejected_callback) const;

        template<typename F_, typename F = pm_any_t>
        inline promise_t then(function<F_(pm_any_t)> fulfilled_callback) const;

        template<typename R_, typename R = pm_any_t>
        inline promise_t fail(function<R_(pm_any_t)> rejected_callback) const;

        template<typename F_, typename F>
        inline promise_t then(function<F_(F)> on_fulfilled) const;

        template<typename F_, typename F, typename R_, typename R>
        inline promise_t then(function<F_(F)> on_fulfilled, function<R_(R)> on_rejected) const;

        template<typename F_, typename F>
        inline promise_t fail(function<F_(F)> on_fulfilled) const;
    };

#define PROMISE_ERR_INVALID_STATE do {throw std::runtime_error("invalid promise state");} while (0)
#define PROMISE_ERR_MISMATCH_TYPE do {throw std::runtime_error("mismatching promise value types");} while (0)
    
    class Promise {
        function<void()> fulfilled_callback;
        function<void()> rejected_callback;
        enum class State {
            Pending,
            Fulfilled,
            Rejected,
        } state;
        pm_any_t result;
        pm_any_t reason;

        void add_on_fulfilled(function<void()> cb) {
            auto old_cbs = fulfilled_callback;
            fulfilled_callback = function<void()>(
                [cb, old_cbs]() {
                    old_cbs();
                    cb();
                });
        }

        void add_on_rejected(function<void()> cb) {
            auto old_cbs = rejected_callback;
            rejected_callback = function<void()>(
                [cb, old_cbs]() {
                    old_cbs();
                    cb();
                });
        }

        function<void()> gen_on_fulfilled(
                function<promise_t(pm_any_t)> on_fulfilled,
                const promise_t &npm) {
            return [this, on_fulfilled, npm]() {
                on_fulfilled(result).then(
                    function<None(pm_any_t)>([npm] (pm_any_t result_) {
                        npm.resolve(result_);
                        return none;
                    }),
                    function<None(pm_any_t)>([npm] (pm_any_t reason_) {
                        npm.reject(reason_);
                        return none;
                    }));
            };
        }

        function<void()> gen_on_fulfilled(
                function<pm_any_t(pm_any_t)> on_fulfilled,
                const promise_t &npm) {
            return [this, on_fulfilled, npm]() {
                npm.resolve(on_fulfilled(result));
            };
        }

        function<void()> gen_on_rejected(
                function<promise_t(pm_any_t)> on_rejected,
                const promise_t &npm) {
            return [this, on_rejected, npm]() {
                on_rejected(reason).then(
                    function<None(pm_any_t)>([npm] (pm_any_t result_) {
                        npm.resolve(result_);
                        return none;
                    }),
                    function<None(pm_any_t)>([npm] (pm_any_t reason_) {
                        npm.reject(reason_);
                        return none;
                    }));
            };
        }

        function<void()> gen_on_rejected(
                function<pm_any_t(pm_any_t)> on_rejected,
                const promise_t &npm) {
            return [this, on_rejected, npm]() {
                npm.reject(on_rejected(reason));
            };
        }

        public:

        Promise():
            fulfilled_callback(do_nothing),
            rejected_callback(do_nothing),
            state(State::Pending) {
            //printf("%lx constructed\n", (uintptr_t)this);
        }

        ~Promise() {
            //printf("%lx freed\n", (uintptr_t)this);
        }

        template<typename OnFulfilled, typename OnRejected>
        promise_t then(function<OnFulfilled(pm_any_t)> on_fulfilled,
                      function<OnRejected(pm_any_t)> on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_fulfilled, on_rejected](promise_t npm) {
                        add_on_fulfilled(gen_on_fulfilled(on_fulfilled, npm));
                        add_on_rejected(gen_on_rejected(on_rejected, npm));
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        gen_on_fulfilled(on_fulfilled, npm)();
                    });
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        gen_on_rejected(on_rejected, npm)();
                    });
                default: PROMISE_ERR_INVALID_STATE;
            }
        }

        template<typename OnFulfilled>
        promise_t then(function<OnFulfilled(pm_any_t)> on_fulfilled) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        add_on_fulfilled(gen_on_fulfilled(on_fulfilled, npm));
                        add_on_rejected([this, npm]() {npm.reject(reason);});
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        gen_on_fulfilled(on_fulfilled, npm)();
                    });
                case State::Rejected:
                    return promise_t([this](promise_t npm) {npm.reject(reason);});
                default: PROMISE_ERR_INVALID_STATE;
            }
        }
 
        template<typename OnRejected>
        promise_t fail(function<OnRejected(pm_any_t)> on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_rejected](promise_t npm) {
                        add_on_rejected(gen_on_rejected(on_rejected, npm));
                        add_on_fulfilled([this, npm]() {npm.resolve(result);});
                    });
                case State::Fulfilled:
                    return promise_t([this](promise_t npm) {npm.resolve(result);});
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        add_on_rejected(gen_on_rejected(on_rejected, npm));
                    });
                default: PROMISE_ERR_INVALID_STATE;
            }
        }
 
        void resolve(pm_any_t _result) {
            switch (state)
            {
                case State::Pending:
                    result = _result;
                    state = State::Fulfilled;
                    fulfilled_callback();
                    break;
                default:
                    break;
            }
        }

        void reject(pm_any_t _reason) {
            switch (state)
            {
                case State::Pending:
                    reason = _reason;
                    state = State::Rejected;
                    rejected_callback();
                    break;
                default:
                    break;
            }
        }
    };
        
    template<typename PList> promise_t all(PList promise_list) {
        auto size = std::make_shared<size_t>(promise_list.size());
        auto results = std::make_shared<values_t>();
        if (!size) PROMISE_ERR_MISMATCH_TYPE;
        results->resize(*size);
        return promise_t([=] (promise_t npm) {
            size_t idx = 0;
            for (const auto &pm: promise_list) {
                pm.then(function<pm_any_t(pm_any_t)>(
                    [results, size, idx, npm](pm_any_t result) {
                        (*results)[idx] = result;
                        if (!--(*size))
                            npm.resolve(*results);
                        return none;
                    }), function<pm_any_t(pm_any_t)>(
                    [npm, size](pm_any_t reason) {
                        npm.reject(reason);
                        return none;
                    }));
                idx++;
            }
        });
    }

    template<typename T>
    inline void promise_t::resolve(T result) const { ptr->resolve(result); }

    template<typename T>
    inline void promise_t::reject(T reason) const { ptr->reject(reason); }

    template<typename F_, typename F, typename R_, typename R>
    inline promise_t promise_t::then(function<F_(pm_any_t)> fulfilled_callback,
                                function<R_(pm_any_t)> rejected_callback) const {
        return ptr->then(fulfilled_callback, rejected_callback);
    }

    template<typename F_, typename F>
    inline promise_t promise_t::then(function<F_(pm_any_t)> fulfilled_callback) const {
        return ptr->then(fulfilled_callback);
    }

    template<typename R_, typename R>
    inline promise_t promise_t::fail(function<R_(pm_any_t)> rejected_callback) const {
        return ptr->fail(rejected_callback);
    }

    template<typename T>
    struct callback_type_conversion {
        using type = pm_any_t;
    };

    template<>
    struct callback_type_conversion<promise_t> {
        using type = promise_t;
    };

    template<typename F_, typename F>
    inline promise_t promise_t::then(function<F_(F)> on_fulfilled) const {
        using ret_type = typename callback_type_conversion<F_>::type;
        return ptr->then(function<ret_type(pm_any_t)>(
            [on_fulfilled](pm_any_t _result) {
                try {
                    return ret_type(on_fulfilled(std::any_cast<F>(_result)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            }));
    }

    template<typename F_, typename F, typename R_, typename R>
    inline promise_t promise_t::then(function<F_(F)> on_fulfilled,
                                    function<R_(R)> on_rejected) const {
        using fulfill_ret_type = typename callback_type_conversion<F_>::type;
        using reject_ret_type = typename callback_type_conversion<R_>::type;
        return ptr->then(function<fulfill_ret_type(pm_any_t)>(
            [on_fulfilled](pm_any_t _result) {
                try {
                    return fulfill_ret_type(on_fulfilled(std::any_cast<F>(_result)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            }), function<fulfill_ret_type(pm_any_t)>(
            [on_rejected](pm_any_t _reason) {
                try {
                    return reject_ret_type(on_rejected(std::any_cast<R>(_reason)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            }));
    }

    template<typename R_, typename R>
    inline promise_t promise_t::fail(function<R_(R)> on_rejected) const {
        using ret_type = typename callback_type_conversion<R_>::type;
        return ptr->fail(function<ret_type(pm_any_t)>(
            [on_rejected](pm_any_t _reason) {
                try {
                    return ret_type(on_rejected(std::any_cast<R>(_reason)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            }));
    }
}
#endif
