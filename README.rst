CPPromise
=========

This is a lightweight C++14/17 compatiable implementation of promises (similar
to Javascript Promise/A+). It allows type-safe polymorphic promises and incurs
little runtime overhead. The runtime type-checking is enforced and supported by
the underlying `any` type, an exception will be thrown when the resolved value
types do not match the types expected in the subsequent computation. See
`test.cpp` for detailed examples.

API
===

::

    typename<typename Func> promise_t(Func callback);

Create a new promise object, the ``callback(promise_t pm)`` is invoked
immediately after the object is constructed, so usually the user registers
``pm`` to some external logic which triggers ``pm.resolve()`` or
``pm.reject()`` when the time comes.

::

    template<typename T> resolve(T result) const;

Resolve the promise with value ``result``. This may trigger the other promises
waiting for the current promise recursively. When a promise is triggered, the
registered ``on_fulfilled()`` function will be invoked using ``result`` as the argument.

::

    tempalte<typename T> reject(T reason) const;

Reject the promise with value ``result``. This may reject the other promises
waiting for the current promise recursively. When a promise is rejected, the
registered ``on_rejected()`` function will be invoked using ``reason`` as the argument.

::

    template<typename FuncFulfilled>
    promise_t then(FuncFulfilled on_fulfilled) const;

Create a new promise that waits for the resolution of the current promise.
``on_fulfilled`` will be called with result from the current promise when
resolved. The rejection will skip the callback and pass on to the promises that
follow the created promise.

::

    template<typename FuncRejected>
    promise_t fail(FuncRejected on_rejected) const;

Create a new promise that waits for the rejection of the current promise.
``on_rejected`` will be called with reason from the current promise when
rejected. The resolution will skip the callback and pass on to the promises
that follow the created promise.

::

    template<typename FuncFulfilled, typename FuncRejected>
    inline promise_t then(FuncFulfilled on_fulfilled,
                            FuncRejected on_rejected) const;

Create a promise that handles both resolution and rejection of the current promise.
