#include "../cornerstone.hxx"
#include <iostream>

#define __decl_test__(name) void test_##name()
#define __run_test__(name)  \
    std::cout << "run test: " << #name << "..." << std::endl;\
    test_##name();\
    std::cout << "test " << #name << " passed." << std::endl

__decl_test__(async_result);
__decl_test__(strfmt);
__decl_test__(buffer);
__decl_test__(serialization);
__decl_test__(scheduler);
__decl_test__(logger);
__decl_test__(log_store);
__decl_test__(raft_server);

int main() {
    __run_test__(async_result);
    __run_test__(strfmt);
    __run_test__(buffer);
    __run_test__(serialization);
    __run_test__(scheduler);
    __run_test__(logger);
    __run_test__(raft_server);
    return 0;
}