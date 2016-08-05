#include "../cornerstone.hxx"
#include <cassert>

using namespace cornerstone;

int __ptr_test_base_calls(0);
int __ptr_test_derived_calls(0);
int __ptr_test_base_destroyed(0);
int __ptr_test_derived_destroyed(0);
int __ptr_test_circular_destroyed(0);

class Base {
public:
    Base(int val) : value_(val) {}

    virtual ~Base() {
        __ptr_test_base_destroyed += 1;
    }

    virtual void func() {
        __ptr_test_base_calls += 1;
    }

    int get_value() const {
        return value_;
    }

private:
    int value_;
};

class Derived : public Base {
public:
    Derived(int val) : Base(val + 10) {}

    virtual ~Derived() {
        __ptr_test_derived_destroyed += 1;
    }

    virtual void func() {
        __ptr_test_derived_calls += 1;
    }
};

class Circular2;

class Circular1 {
public:
    ~Circular1() {
        __ptr_test_circular_destroyed += 1;
    }

    void set_c2(ptr<Circular2>& p) {
        c2_ = p;
    }

private:
    ptr<Circular2> c2_;
};

class Circular2 {
public:
    Circular2(ptr<Circular1>& c1) : c1_(c1) {}

    ~Circular2() {
        __ptr_test_circular_destroyed += 1;
    }

private:
    ptr<Circular1> c1_;
};

void test_ptr() {
    {
        ptr<Base> b(cs_new<Base>(1));
        ptr<Base> b1(cs_new<Derived>(1));
        assert(b->get_value() == 1);
        assert(b1->get_value() == 11);
        b->func();
        b1->func();
        b = b1;
        b->func();

        ptr<Circular1> c1(cs_new<Circular1>());
        ptr<Circular2> c2(cs_new<Circular2>(c1));
        c1->set_c2(c2);
    }

    assert(__ptr_test_base_calls == 1);
    assert(__ptr_test_derived_calls == 2);
    assert(__ptr_test_base_destroyed == 2);
    assert(__ptr_test_derived_destroyed == 1);
    //assert(__ptr_test_circular_destroyed == 2);
}