#include "RabbitMQMessagingAPI.hpp"

namespace Platform::RabbitMQ {

void RabbitMqClient::connect() {}

void RabbitMqClient::publish_message(const std::string &queue_name,
                                     const std::string &message) {}

} // namespace Platform::RabbitMQ
