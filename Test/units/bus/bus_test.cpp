#include <catch2/catch_test_macros.hpp>
#include <numeric>

#include "bus/IMessagingAPI.hpp"
#include "bus/MessagingClient.hpp"

// Write tests for IMessagingAPI::publish_message
// Write tests for IMessagingAPI::connect
// Write tests for MessageBusClient::send_message
// Write tests for MessageBusClient::MessageBusClient
class MockClient : public Core::Bus::IMessagingAPI {
public:
  MockClient(const std::string &hostname, int portid)
      : host(hostname), port(portid) {}

  void connect() override { connected = true; }

  void publish_message(const std::string &queue_name,
                       const std::string &message) override {
    published_messages.push_back(message);
  }
  const std::string &get_host_name() override { return host; }

  Core::i32 get_port() override { return port; }

  std::string host;
  Core::i32 port;
  bool connected = false;
  std::vector<std::string> published_messages;
};

TEST_CASE("Test MessageBusClient") {
  using namespace Core::Bus;

  SECTION("Test MessageBusClient::send_message") {
    Core::Scope<IMessagingAPI> api =
        std::make_unique<MockClient>("localhost", 5672);
    auto client = MessageBusClient(std::move(api));
    client.send_message("test_queue", "test_message");

    // Lets cast it to MockClient to access the published_messages
    auto mock_client = static_cast<MockClient *>(api.get());
    REQUIRE(mock_client->published_messages.size() == 1);
    REQUIRE(mock_client->published_messages[0] == "test_message");
  }

  SECTION("Test MessageBusClient::MessageBusClient") {
    Core::Scope<IMessagingAPI> api =
        std::make_unique<MockClient>("localhost", 5672);
    auto client = MessageBusClient(std::move(api));

    REQUIRE(api->get_host_name() == "localhost");
  }
}
