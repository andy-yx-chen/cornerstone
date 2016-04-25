#include "../cornerstone.hxx"
#include <cstdlib>
#include <cassert>
#include <cstring>

using namespace cornerstone;

void test_strfmt() {
    assert(0 == strcmp("number 10", strfmt<20>("number %d").fmt(10)));
    assert(0 == strcmp("another string", strfmt<30>("another %s").fmt("string")));
    assert(0 == strcmp("100-20=80", strfmt<30>("%d-%d=%d").fmt(100, 20, 80)));
}
