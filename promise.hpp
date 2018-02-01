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

    /* match lambdas */
    template<typename T>
    struct function_traits:
        public function_traits<decltype(&T::operator())> {};
    
    /* match plain functions */
    template<typename ReturnType, typename ArgType>
    struct function_traits<ReturnType(ArgType)> {
        using ret_type = ReturnType;
        using arg_type = ArgType;
    };
  
    /* match function pointers */
    template<typename ReturnType, typename ArgType>
    struct function_traits<ReturnType(*)(ArgType)>:
        public function_traits<ReturnType(ArgType)> {};

    /* match const member functions */
    template<typename ClassType, typename ReturnType, typename ArgType>
    struct function_traits<ReturnType(ClassType::*)(ArgType) const>:
        public function_traits<ReturnType(ArgType)> {};

    /* match member functions */
    template<typename ClassType, typename ReturnType, typename ArgType>
    struct function_traits<ReturnType(ClassType::*)(ArgType)>:
        public function_traits<ReturnType(ArgType)> {};

    class Promise;
    class promise_t {
        std::shared_ptr<Promise> ptr;
        public:

        template<typename Func>
        promise_t(Func callback) {
            ptr = std::make_shared<Promise>();
            callback(*this);
        }

        template<typename T> inline void resolve(T result) const;
        template<typename T> inline void reject(T reason) const;

        template<typename FuncFulfilled, typename FuncRejected>
        inline promise_t then_any(FuncFulfilled fulfilled_callback,
                                FuncRejected rejected_callback) const;

        template<typename FuncFulfilled>
        inline promise_t then_any(FuncFulfilled fulfilled_callback) const;

        template<typename FuncRejected>
        inline promise_t fail_any(FuncRejected rejected_callback) const;

        template<typename FuncFulfilled>
        inline promise_t then(FuncFulfilled on_fulfilled) const;

        template<typename FuncFulfilled, typename FuncRejected>
        inline promise_t then(FuncFulfilled on_fulfilled,
                            FuncRejected on_rejected) const;

        template<typename FuncRejected>
        inline promise_t fail(FuncRejected on_rejected) const;
    };

#define PROMISE_ERR_INVALID_STATE do {throw std::runtime_error("invalid promise state");} while (0)
#define PROMISE_ERR_MISMATCH_TYPE do {throw std::runtime_error("mismatching promise value types");} while (0)
    
    class Promise {
        //function<void()> fulfilled_callback;
        //function<void()> rejected_callback;
        std::vector<function<void()>> fulfilled_callbacks;
        std::vector<function<void()>> rejected_callbacks;
        enum class State {
            Pending,
            Fulfilled,
            Rejected,
        } state;
        pm_any_t result;
        pm_any_t reason;

        /* this implementation causes stack overflow because of the nested lambdas */
        /*
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
        */

        void add_on_fulfilled(function<void()> cb) {
            fulfilled_callbacks.push_back(cb);
        }

        void add_on_rejected(function<void()> cb) {
            rejected_callbacks.push_back(cb);
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         promise_t>::value>::type
        gen_on_fulfilled(Func on_fulfilled, const promise_t &npm, function<void()> &ret) {
            ret = [this, on_fulfilled, npm]() {
                on_fulfilled(result).then_any(
                    [npm] (pm_any_t result_) {
                        npm.resolve(result_);
                        return pm_any_t(none);
                    },
                    [npm] (pm_any_t reason_) {
                        npm.reject(reason_);
                        return pm_any_t(none);
                    });
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         pm_any_t>::value>::type
        gen_on_fulfilled(Func on_fulfilled, const promise_t &npm, function<void()> &ret) {
            ret = [this, on_fulfilled, npm]() {
                npm.resolve(on_fulfilled(result));
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         promise_t>::value>::type
        gen_on_rejected(Func on_rejected, const promise_t &npm, function<void()> &ret) {
            ret = [this, on_rejected, npm]() {
                on_rejected(reason).then_any(
                    [npm] (pm_any_t result_) {
                        npm.resolve(result_);
                        return none;
                    },
                    [npm] (pm_any_t reason_) {
                        npm.reject(reason_);
                        return none;
                    });
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         pm_any_t>::value>::type
        gen_on_rejected(Func on_rejected, const promise_t &npm, function<void()> &ret) {
            ret = [this, on_rejected, npm]() {
                npm.reject(on_rejected(reason));
            };
        }

        public:

        Promise():
            /*
            fulfilled_callback(do_nothing),
            rejected_callback(do_nothing),
            */
            state(State::Pending) {
            //printf("%lx constructed\n", (uintptr_t)this);
        }

        ~Promise() {
            //printf("%lx freed\n", (uintptr_t)this);
        }

        template<typename FuncFulfilled, typename FuncRejected>
        promise_t then(FuncFulfilled on_fulfilled,
                      FuncRejected on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_fulfilled, on_rejected](promise_t npm) {
                        function<void()> ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        add_on_fulfilled(ret);
                        gen_on_rejected(on_rejected, npm, ret);
                        add_on_rejected(ret);
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        function<void()> ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        ret();
                    });
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        function<void()> ret;
                        gen_on_rejected(on_rejected, npm, ret);
                        ret();
                    });
                default: PROMISE_ERR_INVALID_STATE;
            }
        }

        template<typename FuncFulfilled>
        promise_t then(FuncFulfilled on_fulfilled) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        function<void()> ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        add_on_fulfilled(ret);
                        add_on_rejected([this, npm]() {npm.reject(reason);});
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        function<void()> ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        ret();
                    });
                case State::Rejected:
                    return promise_t([this](promise_t npm) {npm.reject(reason);});
                default: PROMISE_ERR_INVALID_STATE;
            }
        }
 
        template<typename FuncRejected>
        promise_t fail(FuncRejected on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_rejected](promise_t npm) {
                        function<void()> ret;
                        gen_on_rejected(on_rejected, npm, ret);
                        add_on_rejected(ret);
                        add_on_fulfilled([this, npm]() {npm.resolve(result);});
                    });
                case State::Fulfilled:
                    return promise_t([this](promise_t npm) {npm.resolve(result);});
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        function<void()> ret;
                        gen_on_rejected(on_rejected, npm, ret);
                        ret();
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
                    //fulfilled_callback();
                    for (const auto &cb: fulfilled_callbacks) cb();
                    rejected_callbacks.clear();
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
                    //rejected_callback();
                    for (const auto &cb: rejected_callbacks) cb();
                    rejected_callbacks.clear();
                    break;
                default:
                    break;
            }
        }
    };
        
    template<typename PList> promise_t all(PList promise_list) {
        return promise_t([promise_list] (promise_t npm) {
            auto size = std::make_shared<size_t>(promise_list.size());
            auto results = std::make_shared<values_t>();
            if (!size) PROMISE_ERR_MISMATCH_TYPE;
            results->resize(*size);
            size_t idx = 0;
            for (const auto &pm: promise_list) {
                pm.then_any(
                    [results, size, idx, npm](pm_any_t result) {
                        (*results)[idx] = result;
                        if (!--(*size))
                            npm.resolve(*results);
                        return pm_any_t(none);
                    },
                    [npm, size](pm_any_t reason) {
                        npm.reject(reason);
                        return pm_any_t(none);
                    });
                idx++;
            }
        });
    }

    template<typename T>
    inline void promise_t::resolve(T result) const { ptr->resolve(result); }

    template<typename T>
    inline void promise_t::reject(T reason) const { ptr->reject(reason); }

    template<typename FuncFulfilled, typename FuncRejected>
    inline promise_t promise_t::then_any(FuncFulfilled fulfilled_callback,
                                    FuncRejected rejected_callback) const {
        return ptr->then(fulfilled_callback, rejected_callback);
    }

    template<typename FuncFulfilled>
    inline promise_t promise_t::then_any(FuncFulfilled fulfilled_callback) const {
        return ptr->then(fulfilled_callback);
    }

    template<typename FuncRejected>
    inline promise_t promise_t::fail_any(FuncRejected rejected_callback) const {
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

    template<typename T>
    struct callback_types {
        using arg_type = typename function_traits<T>::arg_type;
        using ret_type = typename callback_type_conversion<typename function_traits<T>::ret_type>::type;
    };

    template<typename FuncFulfilled>
    inline promise_t promise_t::then(FuncFulfilled on_fulfilled) const {
        using arg_type = typename callback_types<FuncFulfilled>::arg_type;
        using ret_type = typename callback_types<FuncFulfilled>::ret_type;
        return ptr->then(
            [on_fulfilled](pm_any_t _result) {
                try {
                    return ret_type(on_fulfilled(std::any_cast<arg_type>(_result)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }

    template<typename FuncFulfilled, typename FuncRejected>
    inline promise_t promise_t::then(FuncFulfilled on_fulfilled,
                                    FuncRejected on_rejected) const {
        using fulfill_arg_type = typename callback_types<FuncFulfilled>::arg_type;
        using fulfill_ret_type = typename callback_types<FuncFulfilled>::ret_type;
        using reject_arg_type = typename callback_types<FuncRejected>::arg_type;
        using reject_ret_type = typename callback_types<FuncRejected>::ret_type;
        return ptr->then(
            [on_fulfilled](pm_any_t _result) {
                try {
                    return fulfill_ret_type(on_fulfilled(std::any_cast<fulfill_arg_type>(_result)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            },
            [on_rejected](pm_any_t _reason) {
                try {
                    return reject_ret_type(on_rejected(std::any_cast<reject_arg_type>(_reason)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }

    template<typename FuncRejected>
    inline promise_t promise_t::fail(FuncRejected on_rejected) const {
        using arg_type = typename callback_types<FuncRejected>::arg_type;
        using ret_type = typename callback_types<FuncRejected>::ret_type;
        return ptr->fail(
            [on_rejected](pm_any_t _reason) {
                try {
                    return ret_type(on_rejected(std::any_cast<arg_type>(_reason)));
                } catch (std::bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }
}
#endif
