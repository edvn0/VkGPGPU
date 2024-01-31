#include "Allocator.hpp"
#include "Containers.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"
#include "Types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <numeric>

template <class T> using SUT = Core::Container::CircularBuffer<T>;

TEST_CASE("General use cases of a CircularBuffer", "[circular_buffer]") {
  SECTION("Can push 3 elements in a 2 element buffer") {
    SUT<float> two_elements{2};
    two_elements.push(1.F);
    two_elements.push(2.F);
    REQUIRE(two_elements.full());

    two_elements.push(3.F);
    REQUIRE(two_elements.pop() == 2.F);
  }
}

TEST_CASE("CircularBuffer Basic Operations", "[circular_buffer]") {
  SUT<int> buffer(5);

  SECTION("New buffer is empty") {
    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
  }

  SECTION("Pushing items") {
    buffer.push(1);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 1);

    buffer.push(2);
    REQUIRE(buffer.size() == 2);
  }

  SECTION("Popping items") {
    buffer.push(1);
    buffer.push(2);
    REQUIRE(buffer.pop() == 1);
    REQUIRE(buffer.size() == 1);

    REQUIRE(buffer.pop() == 2);
    REQUIRE(buffer.empty());
  }

  SECTION("Buffer wraps correctly") {
    for (int i = 0; i < 6; ++i) {
      buffer.push(i);
    }
    REQUIRE(buffer.size() == 5);
    REQUIRE(buffer.full());
    REQUIRE(buffer.pop() == 1); // First item should have been overwritten
  }

  SECTION("Peek at next item to pop") {
    buffer.push(1);
    buffer.push(2);
    REQUIRE(buffer.peek() == 1);
  }
}

TEST_CASE("CircularBuffer Copy and Move Semantics", "[circular_buffer]") {
  SECTION("Copy constructor") {
    SUT<int> buffer(3);
    buffer.push(1);
    buffer.push(2);

    SUT<int> copy = buffer;
    REQUIRE(copy.size() == 2);
    REQUIRE(copy.pop() == 1);
    REQUIRE(copy.pop() == 2);
  }

  SECTION("Copy assignment") {
    SUT<int> buffer(3);
    buffer.push(1);
    buffer.push(2);

    SUT<int> copy(1);
    copy = buffer;
    REQUIRE(copy.size() == 2);
    REQUIRE(copy.pop() == 1);
    REQUIRE(copy.pop() == 2);
  }

  SECTION("Move constructor") {
    SUT<int> buffer(3);
    buffer.push(1);
    buffer.push(2);

    SUT<int> moved = std::move(buffer);
    REQUIRE(moved.size() == 2);
    REQUIRE(moved.pop() == 1);
    REQUIRE(moved.pop() == 2);
  }

  SECTION("Move assignment") {
    SUT<int> buffer(3);
    buffer.push(1);
    buffer.push(2);

    SUT<int> moved(1);
    moved = std::move(buffer);
    REQUIRE(moved.size() == 2);
    REQUIRE(moved.pop() == 1);
    REQUIRE(moved.pop() == 2);
  }
}

struct ComplexType {
  std::function<void()> func;
  std::vector<int> vec;
  std::array<std::string, 2> arr;
};

TEST_CASE("CircularBuffer with ComplexType", "[circular_buffer]") {
  SUT<ComplexType> buffer(3);

  SECTION("Push and Pop with ComplexType") {
    ComplexType item1;
    item1.func = []() { std::cout << "Function 1\n"; };
    item1.vec = {1, 2, 3};
    item1.arr = {"Hello", "World"};

    ComplexType item2;
    item2.func = []() { std::cout << "Function 2\n"; };
    item2.vec = {4, 5, 6};
    item2.arr = {"Foo", "Bar"};

    buffer.push(item1);
    buffer.push(std::move(item2));

    REQUIRE(buffer.size() == 2);

    auto poppedItem1 = buffer.pop();
    REQUIRE(poppedItem1.vec == std::vector<int>({1, 2, 3}));
    REQUIRE(poppedItem1.arr[0] == "Hello");

    auto poppedItem2 = buffer.pop();
    REQUIRE(poppedItem2.vec == std::vector<int>({4, 5, 6}));
    REQUIRE(poppedItem2.arr[1] == "Bar");

    REQUIRE(buffer.empty());
  }

  SECTION("Copy and Move Semantics with ComplexType") {
    ComplexType item;
    item.func = []() { std::cout << "Function\n"; };
    item.vec = {7, 8, 9};
    item.arr = {"Copy", "Move"};

    buffer.push(item);

    SUT<ComplexType> bufferCopy = buffer;
    REQUIRE(bufferCopy.size() == 1);

    SUT<ComplexType> bufferMoved = std::move(buffer);
    REQUIRE(bufferMoved.size() == 1);
  }
}

TEST_CASE("CircularBuffer emplace", "[circular_buffer]") {
  SUT<ComplexType> buffer(3);

  SECTION("Emplace with ComplexType") {
    buffer.emplace([]() { std::cout << "Function 1\n"; },
                   std::vector<int>({1, 2, 3}),
                   std::array<std::string, 2>({"Hello", "World"}));

    REQUIRE(buffer.size() == 1);

    auto poppedItem1 = buffer.pop();
    REQUIRE(poppedItem1.vec == std::vector<int>({1, 2, 3}));
    REQUIRE(poppedItem1.arr[0] == "Hello");
  }

  SECTION("Emplace with several simple ints") {
    SUT<int> buffer(3);
    buffer.emplace(1);
    buffer.emplace(2);
    buffer.emplace(3);
    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer.pop() == 1);
    REQUIRE(buffer.pop() == 2);
    REQUIRE(buffer.pop() == 3);
  }

  SECTION("Emplace with some different std containers") {
    SUT<std::vector<int>> buffer(3);
    buffer.emplace(std::vector<int>({1, 2, 3}));
    buffer.emplace(std::vector<int>({4, 5, 6}));
    buffer.emplace(std::vector<int>({7, 8, 9}));
    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer.pop() == std::vector<int>({1, 2, 3}));
    REQUIRE(buffer.pop() == std::vector<int>({4, 5, 6}));
    REQUIRE(buffer.pop() == std::vector<int>({7, 8, 9}));

    SUT<std::array<int, 3>> buffer2(3);
    buffer2.emplace(std::array<int, 3>({1, 2, 3}));
    buffer2.emplace(std::array<int, 3>({4, 5, 6}));
    buffer2.emplace(std::array<int, 3>({7, 8, 9}));
    REQUIRE(buffer2.size() == 3);
    REQUIRE(buffer2.pop() == std::array<int, 3>({1, 2, 3}));
    REQUIRE(buffer2.pop() == std::array<int, 3>({4, 5, 6}));
    REQUIRE(buffer2.pop() == std::array<int, 3>({7, 8, 9}));

    SUT<std::string> buffer3(3);
    buffer3.emplace("Hello");
    buffer3.emplace("World");
    buffer3.emplace("Foo");
    REQUIRE(buffer3.size() == 3);
    REQUIRE(buffer3.pop() == "Hello");
    REQUIRE(buffer3.pop() == "World");
    REQUIRE(buffer3.pop() == "Foo");

    SUT<std::array<std::string, 2>> buffer4(3);

    buffer4.emplace(std::array<std::string, 2>({"Hello", "World"}));
    buffer4.emplace(std::array<std::string, 2>({"Foo", "Bar"}));
    buffer4.emplace(std::array<std::string, 2>({"Baz", "Qux"}));
    REQUIRE(buffer4.size() == 3);
    REQUIRE(buffer4.pop() == std::array<std::string, 2>({"Hello", "World"}));
    REQUIRE(buffer4.pop() == std::array<std::string, 2>({"Foo", "Bar"}));
    REQUIRE(buffer4.pop() == std::array<std::string, 2>({"Baz", "Qux"}));
  }
}
