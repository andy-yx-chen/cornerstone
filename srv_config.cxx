#include "cornerstone.hxx"

using namespace cornerstone;

ptr<srv_config> srv_config::deserialize(buffer& buf) {
    int32 id = buf.get_int();
    const char* endpoint = buf.get_str();
    return cs_new<srv_config>(id, endpoint);
}

ptr<buffer> srv_config::serialize() const{
    ptr<buffer> buf = buffer::alloc(sz_int + endpoint_.length() + 1);
    buf->put(id_);
    buf->put(endpoint_);
    buf->pos(0);
    return buf;
}