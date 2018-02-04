CPPromise
=========

.. image:: https://img.shields.io/travis/Determinant/cppromise.svg
   :target: https://github.com/Determinant/cppromise

.. image:: https://img.shields.io/github/license/Determinant/cppromise.svg
   :target: https://github.com/Determinant/cppromise

This is a lightweight C++14/17 compatible implementation of promises (similar
to Javascript Promise/A+). It allows type-safe polymorphic promises and incurs
little runtime overhead. The runtime type-checking is enforced and supported by
the underlying `any` type, an exception will be thrown when the resolved value
types do not match the types expected in the subsequent computation. See
`test.cpp` for detailed examples.

API
===

.. code-block:: cpp

    typename<typename Func> promise_t::promise_t(Func callback);

Create a new promise object, the ``callback(promise_t pm)`` is invoked
immediately after the object is constructed, so usually the user registers
``pm`` to some external logic which triggers ``pm.resolve()`` or
``pm.reject()`` when the time comes.

.. code-block:: cpp

    template<typename T> promise_t::resolve(T result) const;

Resolve the promise with value ``result``. This may trigger the other promises
waiting for the current promise recursively. When a promise is triggered, the
registered ``on_fulfilled()`` function will be invoked with ``result`` as the
argument.

.. code-block:: cpp

    template<typename T> promise_t::reject(T reason) const;

Reject the promise with value ``reason``. This may reject the other promises
waiting for the current promise recursively. When a promise is rejected, the
registered ``on_rejected()`` function will be invoked with ``reason`` as the
argument.

.. code-block:: cpp

    promise_t::resolve() const;

Resolve the promise without empty result. This may trigger the other promises
waiting for the current promise recursively. When a promise is triggered, the
registered ``on_fulfilled()`` function should be expecting no argument,
otherwise a type mismatch is thrown.

.. code-block:: cpp

    promise_t::reject() const;

Reject the promise without empty reason. This may reject the other promises
waiting for the current promise recursively. When a promise is rejected, the
registered ``on_rejected()`` function should be expecting on argument,
otherwise a type mismatch is thrown.

.. code-block:: cpp

    template<typename FuncFulfilled>
    promise_t promise_t::then(FuncFulfilled on_fulfilled) const;

Create a new promise that waits for the resolution of the current promise.
``on_fulfilled`` will be invoked with result from the current promise when
resolved. The rejection will skip the callback and pass on to the promises that
follow the created promise.

.. code-block:: cpp

    template<typename FuncRejected>
    promise_t promise_t::fail(FuncRejected on_rejected) const;

Create a new promise that waits for the rejection of the current promise.
``on_rejected`` will be invoked with reason from the current promise when
rejected. The resolution will skip the callback and pass on to the promises
that follow the created promise.

.. code-block:: cpp

    template<typename FuncFulfilled, typename FuncRejected>
    promise_t promise_t::then(FuncFulfilled on_fulfilled,
                              FuncRejected on_rejected) const;

Create a promise with callbacks that handle both resolution and rejection of
the current promise.

.. code-block:: cpp

    template<typename PList> promise_t promise::all(PList promise_list);

Create a promise waiting for the asynchronous resolution of all promises in
``promise_list``. The result for the created promise will be typed
``values_t``, a vector of ``pm_any_t`` values, each of which being the result
corresponds to a listed promise in ``promise_list`` in order.  The created
promise will be rejected with the reason from the first rejection of any listed
promises.

.. code-block:: cpp

    template<typename PList> promise_t promise::race(PList promise_list);

Create a promise waiting for the asynchronous resolution of any promises in
``promise_list``. The result for the created promise will be the result from
the first resolved promise, and typed ``pm_any_t``.  The created promise will
be rejected with the reason from the first rejection of any listed promises.
