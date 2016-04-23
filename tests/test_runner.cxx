#include "../cornerstone.hxx"
#include <iostream>

#define __decl_test__(name) void test_##name()
#define __run_test__(name)  \
    std::cout << "run test: " << #name << "..." << std::endl;\
    test_##name();\
    std::cout << "test " << #name << " passed." << std::endl

__decl_test__(async_result);
__decl_test__(strfmt);

int main() {
    __run_test__(async_result);
    __run_test__(strfmt);
    return 0;
}