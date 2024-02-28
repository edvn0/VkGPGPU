#pragma once

#include "Concepts.hpp"
#include "Logger.hpp"
#include "Types.hpp"

#include <magic_enum.hpp>
#include <stack>

namespace Core {

template <IsEnum StateEnum> class FiniteStateMachine {
private:
  StateEnum current_state;
  std::stack<StateEnum> backward_stack; // History of past states
  std::stack<StateEnum> forward_stack;  // History of undone states

public:
  FiniteStateMachine(StateEnum initial_state) : current_state(initial_state) {}

  void transition_to(StateEnum new_state) {
    on_leave_state(current_state);
    if (!forward_stack.empty()) {
      clear_forward_stack();
    }
    backward_stack.push(current_state);
    current_state = new_state;
    on_enter_state(new_state);
  }

  void transition_backwards(u32 n = 1) {
    auto count = static_cast<i32>(n);
    while (count-- > 0 && !backward_stack.empty()) {
      on_leave_state(current_state);
      forward_stack.push(current_state);
      current_state = backward_stack.top();
      backward_stack.pop();
      on_enter_state(current_state);
    }
  }

  void transition_forwards(u32 n = 1) {
    auto count = static_cast<i32>(n);

    while (count-- > 0 && !forward_stack.empty()) {
      on_leave_state(current_state);
      backward_stack.push(current_state);
      current_state = forward_stack.top();
      forward_stack.pop();
      on_enter_state(current_state);
    }
  }

  auto get_current_state() const { return current_state; }

protected:
  virtual auto on_enter_state(StateEnum state) -> void {
    // Placeholder for state entry logic
    std::cout << "Entering state: " << magic_enum::enum_name(state)
              << std::endl;
  }
  virtual auto on_leave_state(StateEnum state) -> void {
    // Placeholder for state entry logic
    std::cout << "Leaving state: " << magic_enum::enum_name(state) << std::endl;
  }

private:
  auto clear_forward_stack() -> void {
    while (!forward_stack.empty()) {
      forward_stack.pop();
    }
  }
};

} // namespace Core
