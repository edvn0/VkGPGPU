#include <utility>

#include "bus/IMessagingAPI.hpp"

namespace Platform::RabbitMQ {

class RabbitMQMessagingAPI : public Core::Bus::IMessagingAPI {
public:
  RabbitMQMessagingAPI(std::string hostname, Core::i32 portid)
      : host(std::move(hostname)), port(portid) {}

  void connect() override;
  void publish_message(const std::string &queue_name,
                       const std::string &message) override;
  auto get_port() -> Core::i32 override { return port; }
  auto get_host_name() -> const std::string & override { return host; }

private:
  std::string host;
  Core::i32 port;
};

} // namespace Platform::RabbitMQ
