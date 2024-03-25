#include "FiniteStateMachine.hpp"
#include "Formatters.hpp"

#include <catch2/catch_test_macros.hpp>

#include "catch2/catch_approx.hpp"

using namespace Core;

enum class LifecycleStates { Created, Initialized, Running, Terminated };

enum class TrafficLight { Red, Yellow, Green };

// TestFSM to track on_enter_state and on_leave_state calls
template <typename StateEnum>
class TestFSM : public FiniteStateMachine<StateEnum> {
public:
  std::vector<std::string> log;

  TestFSM(StateEnum initial_state)
      : FiniteStateMachine<StateEnum>(initial_state) {
    on_enter_state(initial_state);
  }

protected:
  void on_enter_state(StateEnum state) override {
    log.push_back("Enter: " + std::string(magic_enum::enum_name(state)));
  }

  void on_leave_state(StateEnum state) override {
    log.push_back("Leave: " + std::string(magic_enum::enum_name(state)));
  }
};

TEST_CASE("LifecycleStates transitions", "[FSM]") {
  FiniteStateMachine<LifecycleStates> fsm(LifecycleStates::Created);

  SECTION("Initial state is Created") {
    REQUIRE(fsm.get_current_state() == LifecycleStates::Created);
  }

  SECTION("Transition from Created to Initialized") {
    fsm.transition_to(LifecycleStates::Initialized);
    REQUIRE(fsm.get_current_state() == LifecycleStates::Initialized);
  }

  SECTION("Transition from Initialized to Running") {
    fsm.transition_to(LifecycleStates::Initialized); // Setup
    fsm.transition_to(LifecycleStates::Running);
    REQUIRE(fsm.get_current_state() == LifecycleStates::Running);
  }
}

TEST_CASE("TrafficLight transitions", "[FSM]") {
  FiniteStateMachine<TrafficLight> fsm(TrafficLight::Red);

  SECTION("Initial state is Red") {
    REQUIRE(fsm.get_current_state() == TrafficLight::Red);
  }

  SECTION("Transition from Red to Green") {
    fsm.transition_to(TrafficLight::Green);
    REQUIRE(fsm.get_current_state() == TrafficLight::Green);
  }

  SECTION("Transition from Green to Yellow") {
    fsm.transition_to(TrafficLight::Green); // Setup
    fsm.transition_to(TrafficLight::Yellow);
    REQUIRE(fsm.get_current_state() == TrafficLight::Yellow);
  }
}

TEST_CASE("FSM Backwards and Forwards Transitions", "[EnhancedFSM]") {
  Core::FiniteStateMachine<LifecycleStates> fsm(LifecycleStates::Created);

  fsm.transition_to(LifecycleStates::Initialized);
  fsm.transition_to(LifecycleStates::Running);

  SECTION("Transition Backwards Once") {
    fsm.transition_backwards();
    REQUIRE(fsm.get_current_state() == LifecycleStates::Initialized);
  }

  SECTION("Transition Backwards Twice Then Forwards") {
    fsm.transition_backwards(2);
    REQUIRE(fsm.get_current_state() == LifecycleStates::Created);

    fsm.transition_forwards();
    REQUIRE(fsm.get_current_state() == LifecycleStates::Initialized);
  }
}

TEST_CASE("FSM on_leave_state and on_enter_state Calls", "[EnhancedFSM]") {
  TestFSM<LifecycleStates> fsm(LifecycleStates::Created);

  fsm.transition_to(LifecycleStates::Initialized);
  fsm.transition_to(LifecycleStates::Running);
  fsm.transition_backwards();

  SECTION("Verify on_leave_state and on_enter_state Calls") {
    std::vector<std::string> expectedLog = {
        "Enter: Created",     "Leave: Created", "Enter: Initialized",
        "Leave: Initialized", "Enter: Running", "Leave: Running",
        "Enter: Initialized"};

    info("{}", fsm.log);

    REQUIRE(fsm.log == expectedLog);
  }
}
