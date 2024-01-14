#pragma once

#include "Types.hpp"

#include <string>

namespace Core::Bus {

class IMessagingAPI {
public:
  virtual ~IMessagingAPI() = default;

  virtual void connect() = 0;
  virtual void publish_message(const std::string &queue_name,
                               const std::string &message) = 0;
  virtual auto get_host_name() -> const std::string & = 0;
  virtual auto get_port() -> Core::i32 = 0;
};

} // namespace Core::Bus
