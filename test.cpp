#include <string>
#include <functional>
#include "promise.hpp"

using callback_t = std::function<void()>;
using promise::promise_t;
using promise::any_cast;

struct A {
    int operator()(int x) {
        printf("operator A got %d\n", x);
        return x + 1;
    }
};

struct B {
    promise_t operator()(int x) {
        printf("operator B got %d\n", x);
        return promise_t([x](promise_t pm) {pm.resolve(x + 1);});
    }
};

int f(int x) {
    printf("plain function f resolved with %d\n", x);
    return x + 1;
}

promise_t g(int x) {
    printf("plain function g resolved with %d\n", x);
    return promise_t([](promise_t pm) {pm.resolve(1);});
}

void test_fac() {
    promise_t root;
    promise_t t = root;
    for (int i = 0; i < 10; i++)
        t = t.then([](std::pair<int, int> p) {
            p.first *= p.second;
            p.second++;
            return p;
        });
    t.then([](std::pair<int, int> p) {
        printf("fac(%d) = %d\n", p.second, p.first);
    });
    root.resolve(std::make_pair(1, 1));
}

int main() {
    callback_t t1;
    callback_t t2;
    callback_t t3;
    callback_t t4;
    callback_t t5;
    A a1, a2, a3;
    B b1, b2, b3;
    auto pm1 = promise_t([&t1](promise_t pm) {
        puts("promise 1 constructed, but won't be resolved immediately");
        t1 = [pm]() {pm.resolve(10);};
    }).then([](int x) {
        printf("got resolved x = %d, output x + 42\n", x);
        return x + 42;
    }).then([](int x) {
        printf("got resolved x = %d, output x * 2\n", x);
        return x * 2;
    }).then([&t2](int x) {
        auto pm2 = promise_t([x, &t2](promise_t pm2) {
            printf("get resolved x = %d, "
                    "promise 2 constructed, not resolved, "
                    "will be resolved with a string instead\n", x);
            t2 = [pm2]() {pm2.resolve(std::string("promise 2 resolved"));};
        });
        return pm2;
    }).then([](std::string s) {
        printf("got string from promise 2: \"%s\", "
                "output 11\n", s.c_str());
        return 11;
    }).then([](int x) {
        printf("got resolved x = %d, output 12\n", x);
        return 12;
    }).then(f).then(a1).fail(a2).then(b1).fail(b2).then(g).then(a3, b3)
    .then([](int) {
        puts("void return is ok");
    }).then([]() {
        puts("void parameter is ok");
        return 1;
    }).then([]() {
        puts("void parameter will ignore the returned value");
    });
    
    auto pm3 = promise_t([&t4](promise_t pm) {
        puts("promise 3 constructed");
        t4 = [pm]() {pm.resolve(1);};
    });

    auto pm4 = promise_t([&t5](promise_t pm) {
        puts("promise 4 constructed");
        t5 = [pm]() {pm.resolve(1.5);};
    });

    auto pm5 = promise_t([&t3](promise_t pm) {
        puts("promise 5 constructed");
        t3 = [pm]() {pm.resolve(std::string("hello world"));};
    });

    auto pm6 = promise::all(std::vector<promise_t>{pm3, pm4, pm5})
        .then([](const promise::values_t values) {
            printf("promise 3, 4, 5 resolved with %d, %.2f, \"%s\"\n",
                    any_cast<int>(values[0]),
                    any_cast<double>(values[1]),
                    any_cast<std::string>(values[2]).c_str());
            return 100;
        });

    auto pm7 = promise::all(std::vector<promise_t>{pm1, pm6})
        .then([](const promise::values_t values) {
            int x = any_cast<int>(values[1]);
            printf("promise 1, 6 resolved %d\n", x);
            return x + 1;
        });

    auto pm8 = promise_t([](promise_t) {
        puts("promsie 8 will never be resolved");
    });

    auto pm9 = promise::race(std::vector<promise_t>{pm7, pm8})
        .then([](promise::pm_any_t value) {
            printf("promise 9 resolved with %d\n",
                    any_cast<int>(value));
        })
        .then([]() {
            puts("rejecting with value -1");
            return promise_t([](promise_t pm) {
                pm.reject(-1);
            });
        })
        .then([]() {
            puts("this line should not appear in the output");
        })
        .then([](int) {
            puts("this line should not appear in the outputs");
        })
        .fail([](int reason) {
            printf("reason: %d\n", reason);
            return reason + 1;
        }).then([](){
            puts("this line should not appear in the outputs");
        },
        [](int reason) {
            printf("reason: %d\n", reason);
        });
    puts("calling t4: resolve promise 3");
    t4();
    puts("calling t5: resolve promise 4");
    t5();
    puts("calling t3: resolve promise 5");
    t3();
    puts("calling t1: resolve first half of promise 1");
    t1();
    puts("calling t2: resolve the second half of promise 1 (promise 2)");
    t2();
    test_fac();
}
