#include "cornerstone.hxx"

using namespace cornerstone;

buffer* cluster_config::serialize() {
    size_t sz = 2 * sz_ulong;
    std::vector<buffer::safe_buffer> srv_buffs;
    for (cluster_config::const_srv_itor it = servers_.begin(); it != servers_.end(); ++it) {
        buffer::safe_buffer buf(std::move(buffer::make_safe((*it)->serialize())));
        srv_buffs.push_back(buf);
        sz += buf->size();
    }

    buffer* result = buffer::alloc(sz);
    result->put(log_idx_);
    result->put(prev_log_idx_);
    for (int i = 0; i < srv_buffs.size(); ++i) {
        result->put(*srv_buffs[i]);
    }

    return result;
}