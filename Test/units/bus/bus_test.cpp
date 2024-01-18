#include <catch2/catch_test_macros.hpp>
#include <numeric>

#include "bus/IMessagingAPI.hpp"
#include "bus/MessagingClient.hpp"

// Write tests for IMessagingAPI::publish_message
// Write tests for IMessagingAPI::connect
// Write tests for MessagingClient::send_message
// Write tests for MessagingClient::MessagingClient
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

TEST_CASE("Test MessagingClient") {
  using namespace Core::Bus;

  SECTION("Test MessagingClient::send_message") {
    Core::Scope<IMessagingAPI> api =
        std::make_unique<MockClient>("localhost", 5672);
    auto client = MessagingClient(std::move(api));
    client.send_message("test_queue", "test_message");

    // Lets cast it to MockClient to access the published_messages
    const auto &mock_client =
        dynamic_cast<const MockClient &>(client.get_api());
    REQUIRE(mock_client.published_messages.size() == 1);
    REQUIRE(mock_client.published_messages[0] == "test_message");
  }

  SECTION("Test MessagingClient::MessagingClient") {
    Core::Scope<IMessagingAPI> api =
        std::make_unique<MockClient>("localhost", 5672);
    auto client = MessagingClient(std::move(api));
    const auto &mock_client =
        dynamic_cast<const MockClient &>(client.get_api());

    REQUIRE(mock_client.host == "localhost");
  }
}
