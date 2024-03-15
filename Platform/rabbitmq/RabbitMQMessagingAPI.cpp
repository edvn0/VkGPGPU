#include "RabbitMQMessagingAPI.hpp"

#include "Logger.hpp"

namespace Platform::RabbitMQ {

void RabbitMQMessagingAPI::connect() {}

void RabbitMQMessagingAPI::publish_message(const std::string &queue_name,
                                           const std::string &message) {
  // trace("Publishing message to queue: {} with message: {}", queue_name,
  //       message);
}

} // namespace Platform::RabbitMQ
