#include <utility>
#include <thread>
#include <chrono>
#include <cassert>
#include "../cornerstone.hxx"

using namespace cornerstone;

typedef async_result<int>::handler_type int_handler;

async_result<int>* create_and_set_async_result(int time_to_sleep, int value, std::exception* err) {
    async_result<int>* result = new async_result<int>();
    if (time_to_sleep <= 0) {
        result->set_result(value, err);
        return result;
    }

    std::thread th([=]() -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(time_to_sleep));
        result->set_result(value, err);
    });

    th.detach();
    return result;
}

void test_async_result() {
    {
        std::unique_ptr<async_result<int>> ptr(create_and_set_async_result(0, 123, nullptr));
        assert(123 == ptr->get());
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called](int val, std::exception* e) -> void {
            assert(123 == val);
            assert(nullptr == e);
            handler_called = true;
	    });
	
        ptr->when_ready(h);
        assert(handler_called);
    }

    {
        std::unique_ptr<async_result<int>> ptr(create_and_set_async_result(200, 496, nullptr));
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called](int val, std::exception* e) -> void {
            assert(496 == val);
            assert(nullptr == e);
            handler_called = true;
	    });
        ptr->when_ready(h);
        assert(496 == ptr->get());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        assert(handler_called);
    }

    {
        std::unique_ptr<async_result<int>> ptr(create_and_set_async_result(200, 496, nullptr));
        assert(496 == ptr->get());
    }

    {
        std::exception* ex = new std::bad_exception();
        std::unique_ptr<async_result<int>> ptr(create_and_set_async_result(200, 496, ex));
        bool handler_called = false;
	    int_handler h = (async_result<int>::handler_type)([&handler_called,ex](int val, std::exception* e) -> void {
            assert(ex == e);
            handler_called = true;
	    });
        ptr->when_ready(h);

        bool ex_handled = false;
        try {
            ptr->get();
        }
        catch (const std::exception* err) {
            (void)err;
            assert(ex == err);
            ex_handled = true;
            delete err;
        }

        assert(ex_handled);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        assert(handler_called);
    }
}
