#include "Allocator.hpp"
#include "Containers.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"
#include "Types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <list>
#include <map>
#include <numeric>
#include <set>
#include <unordered_set>

using namespace Core::Container;

// Test cases
TEST_CASE("Combine vectors of ints") {
  std::vector<int> vec1 = {1, 2, 3};
  std::vector<int> vec2 = {4, 5, 6};

  auto combined = combine_into_one(vec1, vec2);
  REQUIRE(combined == std::vector<int>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("Combine lists of strings") {
  std::list<std::string> list1 = {"a", "b"};
  std::list<std::string> list2 = {"c", "d"};

  auto combined = combine_into_one(list1, list2);
  REQUIRE(combined == std::vector<std::string>{"a", "b", "c", "d"});
}

TEST_CASE("Combine deques of doubles") {
  std::deque<double> deque1 = {1.1, 2.2};
  std::deque<double> deque2 = {3.3, 4.4};

  auto combined = combine_into_one(deque1, deque2);
  REQUIRE(combined == std::vector<double>{1.1, 2.2, 3.3, 4.4});
}

TEST_CASE("Combine sets of chars") {
  std::set<char> set1 = {'a', 'b'};
  std::set<char> set2 = {'c', 'd'};

  auto combined = combine_into_one(set1, set2);
  REQUIRE(combined == std::vector<char>{'a', 'b', 'c', 'd'});
}

TEST_CASE("Combine vector and list of ints") {
  std::vector<int> vec = {1, 2, 3};
  std::list<int> list = {4, 5, 6};

  auto combined = combine_into_one(vec, list);
  REQUIRE(combined == std::vector<int>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("Combine vectors of pairs") {
  std::vector<std::pair<int, std::string>> vec1 = {{1, "one"}, {2, "two"}};
  std::vector<std::pair<int, std::string>> vec2 = {{3, "three"}, {4, "four"}};

  auto combined = combine_into_one(vec1, vec2);
  REQUIRE(combined == std::vector<std::pair<int, std::string>>{
                          {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}});
}

TEST_CASE("Combine lists of vectors") {
  std::list<std::vector<int>> list1 = {{1, 2}, {3, 4}};
  std::list<std::vector<int>> list2 = {{5, 6}, {7, 8}};

  auto combined = combine_into_one(list1, list2);
  REQUIRE(combined ==
          std::vector<std::vector<int>>{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
}

TEST_CASE("Combine deques of sets") {
  std::deque<std::set<int>> deque1 = {{1, 2}, {3}};
  std::deque<std::set<int>> deque2 = {{4}, {5, 6}};

  auto combined = combine_into_one(deque1, deque2);
  REQUIRE(combined == std::vector<std::set<int>>{{1, 2}, {3}, {4}, {5, 6}});
}

struct MyStruct {
  int id;
  std::string name;

  bool operator==(const MyStruct &other) const {
    return id == other.id && name == other.name;
  }
};
TEST_CASE("Combine vectors of custom structs") {
  std::vector<MyStruct> vec1 = {{1, "one"}, {2, "two"}};
  std::vector<MyStruct> vec2 = {{3, "three"}, {4, "four"}};

  auto combined = combine_into_one(vec1, vec2);
  REQUIRE(combined == std::vector<MyStruct>{
                          {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}});
}
TEST_CASE("Combine lists of maps") {
  std::list<std::map<int, std::string>> list1 = {{{1, "one"}, {2, "two"}}};
  std::list<std::map<int, std::string>> list2 = {{{3, "three"}}};

  auto combined = combine_into_one(list1, list2);
  REQUIRE(combined == std::vector<std::map<int, std::string>>{
                          {{1, "one"}, {2, "two"}}, {{3, "three"}}});
}
