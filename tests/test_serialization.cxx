#include "../cornerstone.hxx"
#include <cassert>

using namespace cornerstone;

ulong long_val(int val) {
    ulong base = std::numeric_limits<uint>::max();
    return base + (ulong)val;
}

void test_serialization() {
    uint seed = (uint)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine engine(seed);
    std::uniform_int_distribution<int32> distribution(1, 10000);
    auto rnd = std::bind(distribution, engine);

    std::unique_ptr<srv_config> srv_conf(new srv_config(rnd(), sstrfmt("server %d").fmt(rnd())));
    buffer::safe_buffer srv_conf_buf(buffer::make_safe(srv_conf->serialize()));
    std::unique_ptr<srv_config> srv_conf1(srv_config::deserialize(*srv_conf_buf));
    assert(srv_conf->get_endpoint() == srv_conf1->get_endpoint());
    assert(srv_conf->get_id() == srv_conf1->get_id());

    std::shared_ptr<cluster_config> conf(new cluster_config(long_val(rnd()), long_val(rnd())));
    conf->get_servers().push_back(new srv_config(rnd(), "server 1"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 2"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 3"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 4"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 5"));

    // test cluster config serialization
    buffer::safe_buffer conf_buf(buffer::make_safe(conf->serialize()));
    std::unique_ptr<cluster_config> conf1(cluster_config::deserialize(*conf_buf));
    assert(conf->get_log_idx() == conf1->get_log_idx());
    assert(conf->get_prev_log_idx() == conf1->get_prev_log_idx());
    assert(conf->get_servers().size() == conf1->get_servers().size());
    for (cluster_config::srv_itor it = conf->get_servers().begin(), it1 = conf1->get_servers().begin();
        it != conf->get_servers().end() && it1 != conf1->get_servers().end(); ++it, ++it1) {
        assert((*it)->get_id() == (*it1)->get_id());
        assert((*it)->get_endpoint() == (*it1)->get_endpoint());
    }

    // test snapshot serialization
    std::shared_ptr<snapshot> snp(new snapshot(long_val(rnd()), long_val(rnd()), conf, long_val(rnd())));
    buffer::safe_buffer snp_buf(buffer::make_safe(snp->serialize()));
    std::shared_ptr<snapshot> snp1(snapshot::deserialize(*snp_buf));
    assert(snp->get_last_log_idx() == snp1->get_last_log_idx());
    assert(snp->get_last_log_term() == snp1->get_last_log_term());
    assert(snp->get_last_config()->get_servers().size() == snp1->get_last_config()->get_servers().size());
    assert(snp->get_last_config()->get_log_idx() == snp1->get_last_config()->get_log_idx());
    assert(snp->get_last_config()->get_prev_log_idx() == snp1->get_last_config()->get_prev_log_idx());
    for (cluster_config::srv_itor it = snp->get_last_config()->get_servers().begin(), it1 = snp1->get_last_config()->get_servers().begin();
        it != snp->get_last_config()->get_servers().end() && it1 != snp1->get_last_config()->get_servers().end(); ++it, ++it1) {
        assert((*it)->get_id() == (*it1)->get_id());
        assert((*it)->get_endpoint() == (*it1)->get_endpoint());
    }

    // test snapshot sync request serialization
    bool done = rnd() % 2 == 0;
    buffer* rnd_buf(buffer::alloc(rnd()));
    for (size_t i = 0; i < rnd_buf->size(); ++i) {
        rnd_buf->put((byte)(rnd()));
    }

    rnd_buf->pos(0);

    std::unique_ptr<snapshot_sync_req> sync_req(new snapshot_sync_req(snp, long_val(rnd()), rnd_buf, done));
    buffer::safe_buffer sync_req_buf(buffer::make_safe(sync_req->serialize()));
    std::unique_ptr<snapshot_sync_req> sync_req1(snapshot_sync_req::deserialize(*sync_req_buf));
    assert(sync_req->get_offset() == sync_req1->get_offset());
    assert(done == sync_req1->is_done());
    snapshot& snp2(sync_req1->get_snapshot());
    assert(snp->get_last_log_idx() == snp2.get_last_log_idx());
    assert(snp->get_last_log_term() == snp2.get_last_log_term());
    assert(snp->get_last_config()->get_servers().size() == snp2.get_last_config()->get_servers().size());
    assert(snp->get_last_config()->get_log_idx() == snp2.get_last_config()->get_log_idx());
    assert(snp->get_last_config()->get_prev_log_idx() == snp2.get_last_config()->get_prev_log_idx());
    buffer& buf1 = sync_req1->get_data();
    assert(buf1.pos() == 0);
    assert(rnd_buf->size() == buf1.size());
    for (size_t i = 0; i < buf1.size(); ++i) {
        byte* d = rnd_buf->data();
        byte* d1 = buf1.data();
        assert(*(d + i) == *(d1 + i));
    }

    // test with zero buffer
    done = rnd() % 2 == 1;
    sync_req.reset(new snapshot_sync_req(snp, long_val(rnd()), buffer::alloc(0), done));
    sync_req_buf = buffer::make_safe(sync_req->serialize());
    sync_req1.reset(snapshot_sync_req::deserialize(*sync_req_buf));
    assert(sync_req->get_offset() == sync_req1->get_offset());
    assert(done == sync_req1->is_done());
    assert(sync_req1->get_data().size() == 0);
    snapshot& snp3(sync_req1->get_snapshot());
    assert(snp->get_last_log_idx() == snp3.get_last_log_idx());
    assert(snp->get_last_log_term() == snp3.get_last_log_term());
    assert(snp->get_last_config()->get_servers().size() == snp3.get_last_config()->get_servers().size());
    assert(snp->get_last_config()->get_log_idx() == snp3.get_last_config()->get_log_idx());
    assert(snp->get_last_config()->get_prev_log_idx() == snp3.get_last_config()->get_prev_log_idx());
}

