#include "cornerstone.hxx"

using namespace cornerstone;

srv_config* srv_config::deserialize(buffer& buf) {
    return new srv_config(buf.get_int(), buf.get_str());
}

buffer* srv_config::serialize() {
    buffer* buf = buffer::alloc(sz_int + endpoint_.length() + 1);
    buf->put(id_);
    buf->put(endpoint_);
    buf->pos(0);
    return buf;
}