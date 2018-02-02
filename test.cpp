#include <string>
#include "promise.hpp"
using promise::promise_t;
using promise::any_cast;

struct A {
    int operator()(int x) {
        printf("%d\n", x);
        return 1;
    }
};

struct B {
    promise_t operator()(int x) {
        printf("%d\n", x);
        return promise_t([](promise_t pm) {pm.resolve(1);});
    }
};

int f(int x) {
    printf("%d\n", x);
    return x + 1;
}

promise_t g(int x) {
    printf("%d\n", x);
    return promise_t([](promise_t pm) {pm.resolve(1);});
}

int main() {
    std::function<void()> t1;
    std::function<void()> t2;
    std::function<void()> t3;
    std::function<void()> t4;
    std::function<void()> t5;
    A a1, a2, a3;
    B b1, b2, b3;
    auto pm = promise_t([&t1](promise_t pm) {
        puts("pm1");
        //t1 = [pm]() {pm.reject(5);};
        t1 = [pm]() {pm.resolve(5);};
    }).then([](int x) {
        printf("%d\n", x);
        return 6;
    }).then([](int y) {
        printf("%d\n", y);
        return 0;
    }).then([&t2](int x) {
        auto pm2 = promise_t([x, &t2](promise_t pm2) {
            printf("pm2 %d\n", x);
            t2 = [pm2]() {pm2.resolve(std::string("hello"));};
        });
        return pm2;
    }).then([](std::string s) {
        printf("%s\n", s.c_str());
        return 10;
    }).then([](int x) {
        printf("%d\n", x);
        return 12;
    }).then(f).then(a1).fail(a2).then(b1).fail(b2).then(g).then(a3, b3);
    
    auto p1 = promise_t([&t4](promise_t pm) {
        puts("p1");
        t4 = [pm]() {pm.resolve(1);};
    });

    auto p2 = promise_t([&t5](promise_t pm) {
        puts("p2");
        t5 = [pm]() {pm.resolve(std::string("hello"));};
    });

    auto p3 = promise_t([&t3](promise_t pm) {
        puts("p3");
        t3 = [pm]() {pm.resolve(std::string("world"));};
    });

    auto p4 = promise::all(std::vector<promise_t>{p1, p2, p3})
        .then([](const promise::values_t values) {
            printf("%d %s %s\n", any_cast<int>(values[0]),
                                any_cast<std::string>(values[1]).c_str(),
                                any_cast<std::string>(values[2]).c_str());
            return 100;
        });

    auto p5 = promise::all(std::vector<promise_t>{pm, p4})
        .fail([](int reason) {
            printf("reason: %d\n", reason);
            return reason;
        })
        .then([](const promise::values_t values) {
            printf("finally %d\n", any_cast<int>(values[1]));
            return promise::none;
        });
    puts("calling t");
    t4();
    puts("calling t2");
    t5();
    puts("calling t3");
    t3();

    t1();
    printf("=== after ===\n");
    t2();
}
