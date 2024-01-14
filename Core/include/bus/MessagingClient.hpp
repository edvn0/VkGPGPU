#include "bus/IMessagingAPI.hpp"

namespace Core::Bus {

class MessageBusClient {
  std::unique_ptr<IMessagingAPI> messagingAPI;

public:
  explicit MessageBusClient(std::unique_ptr<IMessagingAPI> api)
      : messagingAPI(std::move(api)) {
    messagingAPI->connect();
  }

  void send_message(const std::string &queueName, const std::string &message) {
    messagingAPI->publish_message(queueName, message);
  }
};

} // namespace Core::Bus
