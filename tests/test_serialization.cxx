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

    std::shared_ptr<cluster_config> conf(new cluster_config(long_val(rnd()), long_val(rnd())));
    conf->get_servers().push_back(new srv_config(rnd(), "server 1"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 2"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 3"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 4"));
    conf->get_servers().push_back(new srv_config(rnd(), "server 5"));

    // test snapshot serialization
    std::shared_ptr<snapshot> snp(new snapshot(long_val(rnd()), long_val(rnd()), conf, long_val(rnd())));
    buffer::safe_buffer snp_buf(buffer::make_safe(snp->serialize()));
    std::shared_ptr<snapshot> snp1(snapshot::deserialize(*snp_buf));
    assert(snp->get_last_log_idx() == snp1->get_last_log_idx());
    assert(snp->get_last_log_term() == snp1->get_last_log_term());
    assert(snp->get_last_config()->get_servers().size() == snp1->get_last_config()->get_servers().size());
    assert(snp->get_last_config()->get_log_idx() == snp1->get_last_config()->get_log_idx());
    assert(snp->get_last_config()->get_prev_log_idx() == snp1->get_last_config()->get_prev_log_idx());
    //to add more
}

