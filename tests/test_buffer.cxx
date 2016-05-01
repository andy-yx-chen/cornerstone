#include "../cornerstone.hxx"
#include <cassert>

using namespace cornerstone;

static void do_test(buffer* buf);

void test_buffer() {
	
	buffer* buf = buffer::alloc(1024);
	assert(buf->size() == 1024);
	do_test(buf);
	buffer::release(buf);

	buf = buffer::alloc(0x10000);
	assert(buf->size() == 0x10000);
	do_test(buf);
	buffer::release(buf);
}

static void do_test(buffer* buf) {
	uint seed = (uint)std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine engine(seed);
	std::uniform_int_distribution<int32> distribution(1, 10000);
	auto rnd = std::bind(distribution, engine);

	// store int32 values into buffer
	std::vector<int32> vals;
	for (int i = 0; i < 100; ++i) {
		int32 val = rnd();
		vals.push_back(val);
		buf->put(val);
	}

	assert(buf->pos() == 100 * sz_int);
	buf->pos(0);
	for (int i = 0; i < 100; ++i) {
		int32 val = buf->get_int();
		assert(val == vals[i]);
	}
}