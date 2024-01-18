#pragma once

#include "bus/IMessagingAPI.hpp"

namespace Core::Bus {

class MessagingClient {

public:
  explicit MessagingClient(std::unique_ptr<IMessagingAPI> api)
      : messagingAPI(std::move(api)) {
    messagingAPI->connect();
  }

  void send_message(const std::string &queue_name,
                    const std::string &message) const {
    messagingAPI->publish_message(queue_name, message);
  }

  auto get_api() const -> const IMessagingAPI & { return *messagingAPI; }

private:
  std::unique_ptr<IMessagingAPI> messagingAPI;
};

} // namespace Core::Bus
