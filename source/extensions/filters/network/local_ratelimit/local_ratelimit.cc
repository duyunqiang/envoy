#include "extensions/filters/network/local_ratelimit/local_ratelimit.h"

#include "envoy/event/dispatcher.h"

#include "common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace LocalRateLimitFilter {

Config::Config(
    const envoy::config::filter::network::local_rate_limit::v2alpha::LocalRateLimit& proto_config,
    Event::Dispatcher& dispatcher, Stats::Scope& scope, Runtime::Loader& runtime)
    : fill_timer_(dispatcher.createTimer([this] { onFillTimer(); })),
      max_tokens_(proto_config.token_bucket().max_tokens()),
      tokens_per_fill_(
          PROTOBUF_GET_WRAPPED_OR_DEFAULT(proto_config.token_bucket(), tokens_per_fill, 1)),
      fill_interval_(PROTOBUF_GET_MS_REQUIRED(proto_config.token_bucket(), fill_interval)),
      enabled_(proto_config.enabled(), runtime),
      stats_(generateStats(proto_config.stat_prefix(), scope)), tokens_(max_tokens_) {
  fill_timer_->enableTimer(fill_interval_);
}

LocalRateLimitStats Config::generateStats(const std::string& prefix, Stats::Scope& scope) {
  const std::string final_prefix = "local_rate_limit." + prefix;
  return {ALL_LOCAL_RATE_LIMIT_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
}

void Config::onFillTimer() {
  // Relaxed consistency is used for all operations because we don't care about ordering, just the
  // final atomic correctness.
  uint32_t expected_tokens = tokens_.load(std::memory_order_relaxed);
  uint32_t new_tokens_value;
  do {
    // expected_tokens is either initialized above or reloaded during the CAS failure below.
    new_tokens_value = std::min(max_tokens_, expected_tokens + tokens_per_fill_);

    // Loop while the weak CAS fails trying to update the tokens value.
  } while (
      !tokens_.compare_exchange_weak(expected_tokens, new_tokens_value, std::memory_order_relaxed));

  ENVOY_LOG(trace, "local_rate_limit: fill tokens={}", new_tokens_value);
  fill_timer_->enableTimer(fill_interval_);
}

bool Config::canCreateConnection() {
  // Relaxed consistency is used for all operations because we don't care about ordering, just the
  // final atomic correctness.
  uint32_t expected_tokens = tokens_.load(std::memory_order_relaxed);
  do {
    // expected_tokens is either initialized above or reloaded during the CAS failure below.
    if (expected_tokens == 0) {
      return false;
    }

    // Loop while the weak CAS fails trying to subtract 1 from expected.
  } while (!tokens_.compare_exchange_weak(expected_tokens, expected_tokens - 1,
                                          std::memory_order_relaxed));

  // We successfully decremented the counter by 1.
  return true;
}

Network::FilterStatus Filter::onNewConnection() {
  if (!config_->enabled()) {
    ENVOY_CONN_LOG(trace, "local_rate_limit: runtime disabled", read_callbacks_->connection());
    return Network::FilterStatus::Continue;
  }

  if (!config_->canCreateConnection()) {
    config_->stats().rate_limit_.inc();
    ENVOY_CONN_LOG(trace, "local_rate_limit: rate limiting connection",
                   read_callbacks_->connection());
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    return Network::FilterStatus::StopIteration;
  }

  return Network::FilterStatus::Continue;
}

} // namespace LocalRateLimitFilter
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy