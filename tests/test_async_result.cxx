#include <utility>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "../cornerstone.hxx"

using namespace cornerstone;

typedef async_result<int>::handler_type int_handler;

ptr<std::exception> no_except;

ptr<async_result<int>> create_and_set_async_result(int time_to_sleep, int value, ptr<std::exception>& err) {
    ptr<async_result<int>> result(cs_new<async_result<int>>());
    if (time_to_sleep <= 0) {
        result->set_result(value, err);
        return result;
    }

    std::thread th([=]() -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(time_to_sleep));
        int val = value;
        ptr<std::exception> ex(err);
        result->set_result(val, ex);
    });

    th.detach();
    return result;
}

void test_async_result() {
    std::cout << "test with sync set" << std::endl;
    {
        ptr<async_result<int>> p(create_and_set_async_result(0, 123, no_except));
        assert(123 == p->get());
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called](int val, ptr<std::exception>& e) -> void {
            assert(123 == val);
            assert(e == nullptr);
            handler_called = true;
	    });
	
        p->when_ready(h);
        assert(handler_called);
    }

    std::cout << "test with async set and wait" << std::endl;
    {
        ptr<async_result<int>> presult(create_and_set_async_result(200, 496, no_except));
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called](int val, ptr<std::exception> e) -> void {
            assert(496 == val);
            assert(e == nullptr);
            handler_called = true;
	    });
        presult->when_ready(h);
        assert(496 == presult->get());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        assert(handler_called);
    }

    std::cout << "test with async set and wait without completion handler" << std::endl;
    {
        ptr<async_result<int>> p(create_and_set_async_result(200, 496, no_except));
        assert(496 == p->get());
    }

    std::cout << "test with exceptions" << std::endl;
    {
        ptr<std::exception> ex = cs_new<std::bad_exception>();
        ptr<async_result<int>> presult(create_and_set_async_result(200, 496, ex));
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called,ex](int val, ptr<std::exception>& e) -> void {
            assert(ex == e);
            handler_called = true;
	    });
        presult->when_ready(h);

        bool ex_handled = false;
        try {
            presult->get();
        }
        catch (const ptr<std::exception>& err) {
            (void)err;
            assert(ex == err);
            ex_handled = true;
        }

        assert(ex_handled);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        assert(handler_called);
    }
}
