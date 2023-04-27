/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <map>
#include <memory>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace zen {

class Prometheus {
  public:
    explicit Prometheus(std::string listen_address);
    ~Prometheus() = default;

    prometheus::Registry& registry() { return *registry_; }

    [[nodiscard]] prometheus::Family<prometheus::Counter>& set_counter(const std::string& name,
                                                                       const std::string& help) {
        return prometheus::BuildCounter().Name(name).Help(help).Register(*registry_);
    }
    [[nodiscard]] prometheus::Family<prometheus::Gauge>& set_gauge(const std::string& name, const std::string& help) {
        return prometheus::BuildGauge().Name(name).Help(help).Register(*registry_);
    }
    [[nodiscard]] prometheus::Family<prometheus::Histogram>& set_histogram(const std::string& name,
                                                                           const std::string& help) {
        return prometheus::BuildHistogram().Name(name).Help(help).Register(*registry_);
    }

  private:
    std::unique_ptr<prometheus::Exposer> exposer_;    // Prometheus server
    std::shared_ptr<prometheus::Registry> registry_;  // Prometheus Registry
};
}  // namespace zen
