/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "prometheus.hpp"

#include <iostream>

namespace zen {
using namespace prometheus;
Prometheus::Prometheus(std::string listen_address) {
    exposer_ = std::make_unique<prometheus::Exposer>(listen_address);
    registry_ = std::make_shared<prometheus::Registry>();
    exposer_->RegisterCollectable(registry_);

    //    auto& packet_counter =
    //        BuildCounter().Name("observed_packets_total").Help("Number of observed packets").Register(*registry_);
    //
    //
    //    // add and remember dimensional data, incrementing those is very cheap
    //    packet_counter.Add({{"protocol", "tcp"}, {"direction", "rx"}});
    //    packet_counter.Add({{"protocol", "tcp"}, {"direction", "tx"}});
    //    packet_counter.Add({{"protocol", "udp"}, {"direction", "rx"}});
    //    packet_counter.Add({{"protocol", "udp"}, {"direction", "tx"}});
    //
    //
    //
    //
    //    for(const auto& m : registry_->Collect()) {
    //        std::cout << m.name << std::endl;
    //    }
}
}  // namespace zen
