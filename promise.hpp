#ifndef _CPPROMISE_HPP
#define _CPPROMISE_HPP

/**
 * MIT License
 * Copyright (c) 2018 Ted Yin <tederminant@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <vector>
#include <memory>
#include <functional>

#if __has_include("any")
#include <any>
#endif

#if !defined(__cpp_lib_any)
#include <boost/any.hpp>
#endif

/**
 * Implement type-safe Promise primitives similar to the ones specified by
 * Javascript Promise/A+.
 */
namespace promise {
#if defined(__cpp_lib_any)
    using pm_any_t = std::any;
    template<typename T>
    constexpr auto any_cast = static_cast<T(*)(const std::any&)>(std::any_cast<T>);
    using bad_any_cast = std::bad_any_cast;
#else
#warning "using any type from boost"
    using pm_any_t = boost::any;
    template<typename T>
    constexpr auto any_cast = static_cast<T(*)(const boost::any&)>(boost::any_cast<T>);
    using bad_any_cast = boost::bad_any_cast;
#endif
    struct None {};
    const auto none = None();
    using callback_t = std::function<void()>;
    using values_t = std::vector<pm_any_t>;

    /* match lambdas */
    template<typename T>
    struct function_traits:
        public function_traits<decltype(&T::operator())> {};
 
    template<typename ReturnType>
    struct function_traits<ReturnType()> {
        using ret_type = ReturnType;
        using arg_type = None;
        using empty_arg = void;
    };
   
    /* match plain functions */
    template<typename ReturnType, typename ArgType>
    struct function_traits<ReturnType(ArgType)> {
        using ret_type = ReturnType;
        using arg_type = ArgType;
        using non_empty_arg = void;
    };
 
    /* match function pointers */
    template<typename ReturnType, typename... ArgType>
    struct function_traits<ReturnType(*)(ArgType...)>:
        public function_traits<ReturnType(ArgType...)> {};

    /* match const member functions */
    template<typename ClassType, typename ReturnType, typename... ArgType>
    struct function_traits<ReturnType(ClassType::*)(ArgType...) const>:
        public function_traits<ReturnType(ArgType...)> {};

    /* match member functions */
    template<typename ClassType, typename ReturnType, typename... ArgType>
    struct function_traits<ReturnType(ClassType::*)(ArgType...)>:
        public function_traits<ReturnType(ArgType...)> {};

    class Promise;
    class promise_t: std::shared_ptr<Promise> {
        public:
        friend Promise;
        template<typename PList> friend promise_t all(PList promise_list);
        template<typename Func>
        promise_t(Func callback):
            std::shared_ptr<Promise>(std::make_shared<Promise>()) {
            callback(*this);
        }

        template<typename T> inline void resolve(T result) const;
        template<typename T> inline void reject(T reason) const;

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
        std::vector<callback_t> fulfilled_callbacks;
        std::vector<callback_t> rejected_callbacks;
        enum class State {
            Pending,
            Fulfilled,
            Rejected,
        } state;
        pm_any_t result;
        pm_any_t reason;

        void add_on_fulfilled(callback_t cb) {
            fulfilled_callbacks.push_back(cb);
        }

        void add_on_rejected(callback_t cb) {
            rejected_callbacks.push_back(cb);
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         promise_t>::value>::type
        gen_on_fulfilled(Func on_fulfilled, const promise_t &npm, callback_t &ret) {
            ret = [this, on_fulfilled, npm]() mutable {
                on_fulfilled(result)->then(
                    [npm] (pm_any_t result_) {
                        npm->resolve(result_);
                        return pm_any_t(none);
                    },
                    [npm] (pm_any_t reason_) {
                        npm->reject(reason_);
                        return pm_any_t(none);
                    });
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         pm_any_t>::value>::type
        gen_on_fulfilled(Func on_fulfilled, const promise_t &npm, callback_t &ret) {
            ret = [this, on_fulfilled, npm]() mutable {
                npm->resolve(on_fulfilled(result));
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         promise_t>::value>::type
        gen_on_rejected(Func on_rejected, const promise_t &npm, callback_t &ret) {
            ret = [this, on_rejected, npm]() mutable {
                on_rejected(reason)->then(
                    [npm] (pm_any_t result_) {
                        npm->resolve(result_);
                        return pm_any_t(none);
                    },
                    [npm] (pm_any_t reason_) {
                        npm->reject(reason_);
                        return pm_any_t(none);
                    });
            };
        }

        template<typename Func>
        typename std::enable_if<
            std::is_same<typename function_traits<Func>::ret_type,
                         pm_any_t>::value>::type
        gen_on_rejected(Func on_rejected, const promise_t &npm, callback_t &ret) {
            ret = [this, on_rejected, npm]() mutable {
                npm->reject(on_rejected(reason));
            };
        }

        public:

        Promise(): state(State::Pending) {}
        ~Promise() {}

        template<typename FuncFulfilled, typename FuncRejected>
        promise_t then(FuncFulfilled on_fulfilled,
                      FuncRejected on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_fulfilled, on_rejected](promise_t npm) {
                        callback_t ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        add_on_fulfilled(ret);
                        gen_on_rejected(on_rejected, npm, ret);
                        add_on_rejected(ret);
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        callback_t ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        ret();
                    });
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        callback_t ret;
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
                        callback_t ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        add_on_fulfilled(ret);
                        add_on_rejected([this, npm]() {npm->reject(reason);});
                    });
                case State::Fulfilled:
                    return promise_t([this, on_fulfilled](promise_t npm) {
                        callback_t ret;
                        gen_on_fulfilled(on_fulfilled, npm, ret);
                        ret();
                    });
                case State::Rejected:
                    return promise_t([this](promise_t npm) {npm->reject(reason);});
                default: PROMISE_ERR_INVALID_STATE;
            }
        }
 
        template<typename FuncRejected>
        promise_t fail(FuncRejected on_rejected) {
            switch (state)
            {
                case State::Pending:
                    return promise_t([this, on_rejected](promise_t npm) {
                        callback_t ret;
                        gen_on_rejected(on_rejected, npm, ret);
                        add_on_rejected(ret);
                        add_on_fulfilled([this, npm]() {npm->resolve(result);});
                    });
                case State::Fulfilled:
                    return promise_t([this](promise_t npm) {npm->resolve(result);});
                case State::Rejected:
                    return promise_t([this, on_rejected](promise_t npm) {
                        callback_t ret;
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
                pm->then(
                    [results, size, idx, npm](pm_any_t result) {
                        (*results)[idx] = result;
                        if (!--(*size))
                            npm->resolve(*results);
                        return pm_any_t(none);
                    },
                    [npm, size](pm_any_t reason) {
                        npm->reject(reason);
                        return pm_any_t(none);
                    });
                idx++;
            }
        });
    }

    template<typename T>
    inline void promise_t::resolve(T result) const { (*this)->resolve(result); }

    template<typename T>
    inline void promise_t::reject(T reason) const { (*this)->reject(reason); }

    template<typename T>
    struct callback_types {
        using arg_type = typename function_traits<T>::arg_type;
        using ret_type = typename std::conditional<
            std::is_same<typename function_traits<T>::ret_type, promise_t>::value,
            promise_t, pm_any_t>::type;
    };

    template<typename Func,
            typename std::enable_if<std::is_same<
                typename function_traits<Func>::ret_type,
                void>::value>::type * = nullptr>
    constexpr auto convert_void_func_(Func f) {
        return [f](typename function_traits<Func>::arg_type v) {
            f(v);
            return none;
        };
    }
    
    template<typename Func,
            typename std::enable_if<!std::is_same<
                typename function_traits<Func>::ret_type,
                void>::value>::type * = nullptr>
    constexpr auto convert_void_func_(Func f) { return f; }

    template<typename Func, typename function_traits<Func>::empty_arg * = nullptr>
    constexpr auto convert_void_func(Func f) { return convert_void_func_([f](None) {f();}); }

    template<typename Func, typename function_traits<Func>::non_empty_arg * = nullptr>
    constexpr auto convert_void_func(Func f) { return convert_void_func_(f); }

    template<typename FuncFulfilled>
    inline promise_t promise_t::then(FuncFulfilled on_fulfilled) const {
        using fulfill_t = callback_types<FuncFulfilled>;
        return (*this)->then(
            [on_fulfilled](pm_any_t _result) mutable {
                try {
                    return typename fulfill_t::ret_type(convert_void_func(on_fulfilled)(
                        any_cast<typename fulfill_t::arg_type>(_result)));
                } catch (bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }

    template<typename FuncFulfilled, typename FuncRejected>
    inline promise_t promise_t::then(FuncFulfilled on_fulfilled,
                                    FuncRejected on_rejected) const {
        using fulfill_t = callback_types<FuncFulfilled>;
        using reject_t = callback_types<FuncRejected>;
        return (*this)->then(
            [on_fulfilled](pm_any_t _result) mutable {
                try {
                    return typename fulfill_t::ret_type(convert_void_func(on_fulfilled)(
                        any_cast<typename fulfill_t::arg_type>(_result)));
                } catch (bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            },
            [on_rejected](pm_any_t _reason) mutable {
                try {
                    return typename reject_t::ret_type(convert_void_func(on_rejected)(
                        any_cast<typename reject_t::arg_type>(_reason)));
                } catch (bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }

    template<typename FuncRejected>
    inline promise_t promise_t::fail(FuncRejected on_rejected) const {
        using reject_t = callback_types<FuncRejected>;
        return (*this)->fail(
            [on_rejected](pm_any_t _reason) mutable {
                try {
                    return typename reject_t::ret_type(convert_void_func(on_rejected)(
                        any_cast<typename reject_t::arg_type>(_reason)));
                } catch (bad_any_cast e) { PROMISE_ERR_MISMATCH_TYPE; }
            });
    }
}

#endif
