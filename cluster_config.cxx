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

cluster_config* cluster_config::deserialize(buffer& buf) {
    try {
        ulong log_idx = buf.get_ulong();
        ulong prev_log_idx = buf.get_ulong();
        cluster_config* conf = new cluster_config(log_idx, prev_log_idx);
        while (buf.pos() < buf.size()) {
            conf->get_servers().push_back(srv_config::deserialize(buf));
        }

        return conf;
    }
    catch (std::overflow_error& err) {
        return nilptr;
    }
}