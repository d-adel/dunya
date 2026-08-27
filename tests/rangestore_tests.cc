#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/rangestore/rangestore.h>

#include <cstdint>

namespace {

using Store = dunya::objectmodel::RangeStore<uint32_t>;
using Range = Store::Range;

constexpr uint32_t POOL = 64;
constexpr uint32_t MAX_RANGE = 32;

// Fills a range with a marker so a later read can prove the elements moved
// rather than merely that the offsets changed.
void fill(Store& store, Range range, uint32_t count, uint32_t first) {
  std::span<uint32_t> elements = store.at(range, count);

  for (uint32_t i = 0; i != count; ++i) {
    elements[i] = first + i;
  }
}

}  // namespace

TEST_CASE("an allocation grows the pool", "[rangestore]") {
  Store store(POOL);

  const std::optional<Range> range = store.grow(Range{}, 0, 4, MAX_RANGE);

  REQUIRE(range.has_value());
  REQUIRE(range->offset == 0);
  REQUIRE(range->capacity == 4);
  REQUIRE(store.size() == 4);
}

TEST_CASE("releasing the last range shrinks the pool", "[rangestore]") {
  // Not merely freed: a pool that empties returns to zero rather than leaving
  // one arena-sized hole behind.
  Store store(POOL);

  const Range first = *store.grow(Range{}, 0, 4, MAX_RANGE);
  const Range second = *store.grow(Range{}, 0, 8, MAX_RANGE);

  store.release(second);
  REQUIRE(store.size() == 4);

  store.release(first);
  REQUIRE(store.size() == 0);
  REQUIRE(store.freeRangeCount() == 0);
}

TEST_CASE("a hole in the middle is reused", "[rangestore]") {
  Store store(POOL);

  const Range first = *store.grow(Range{}, 0, 4, MAX_RANGE);
  store.grow(Range{}, 0, 4, MAX_RANGE);

  store.release(first);
  REQUIRE(store.freeRangeCount() == 1);

  const std::optional<Range> reused = store.grow(Range{}, 0, 4, MAX_RANGE);

  REQUIRE(reused.has_value());
  REQUIRE(reused->offset == first.offset);
  REQUIRE(store.size() == 8);
}

TEST_CASE("adjacent free ranges coalesce", "[rangestore]") {
  // The reason release walks its neighbours. Without it, freeing two abutting
  // ranges leaves two holes and an allocation the size of both fails against a
  // pool that has room.
  Store store(POOL);

  const Range first = *store.grow(Range{}, 0, 4, MAX_RANGE);
  const Range second = *store.grow(Range{}, 0, 4, MAX_RANGE);
  store.grow(Range{}, 0, 4, MAX_RANGE);

  store.release(first);
  store.release(second);

  REQUIRE(store.freeRangeCount() == 1);

  const std::optional<Range> combined = store.grow(Range{}, 0, 8, MAX_RANGE);

  REQUIRE(combined.has_value());
  REQUIRE(combined->offset == first.offset);
  REQUIRE(store.size() == 12);
}

TEST_CASE("a hole coalesces with the range after it", "[rangestore]") {
  // The other direction: released last, freed first. Both neighbours have to
  // be checked, and only one of the two checks would catch this.
  Store store(POOL);

  const Range first = *store.grow(Range{}, 0, 4, MAX_RANGE);
  const Range second = *store.grow(Range{}, 0, 4, MAX_RANGE);
  store.grow(Range{}, 0, 4, MAX_RANGE);

  store.release(second);
  store.release(first);

  REQUIRE(store.freeRangeCount() == 1);
  REQUIRE(store.grow(Range{}, 0, 8, MAX_RANGE).has_value());
}

TEST_CASE("allocation takes the smallest hole that fits", "[rangestore]") {
  // Best fit, not first fit: spending a large hole on a small request is how
  // an arena with room starts refusing allocations.
  Store store(POOL);

  const Range big = *store.grow(Range{}, 0, 16, MAX_RANGE);
  store.grow(Range{}, 0, 4, MAX_RANGE);
  const Range small = *store.grow(Range{}, 0, 8, MAX_RANGE);
  store.grow(Range{}, 0, 4, MAX_RANGE);

  store.release(big);
  store.release(small);

  REQUIRE(store.freeRangeCount() == 2);

  const std::optional<Range> fitted = store.grow(Range{}, 0, 8, MAX_RANGE);

  REQUIRE(fitted.has_value());
  REQUIRE(fitted->offset == small.offset);
}

TEST_CASE("growing moves the elements and frees the old range",
          "[rangestore]") {
  Store store(POOL);

  const Range range = *store.grow(Range{}, 0, 4, MAX_RANGE);
  fill(store, range, 4, 100);

  store.grow(Range{}, 0, 4, MAX_RANGE);

  const std::optional<Range> grown = store.grow(range, 4, 5, MAX_RANGE);

  REQUIRE(grown.has_value());
  REQUIRE(grown->capacity == 8);
  REQUIRE(grown->offset != range.offset);

  const std::span<const uint32_t> elements = store.at(*grown, 4);

  REQUIRE(elements[0] == 100);
  REQUIRE(elements[3] == 103);

  // The vacated range is free again, so a four-element request reuses it.
  const std::optional<Range> reused = store.grow(Range{}, 0, 4, MAX_RANGE);
  REQUIRE(reused.has_value());
  REQUIRE(reused->offset == range.offset);
}

TEST_CASE("capacity doubles from four", "[rangestore]") {
  // Observed through grow rather than by asking for the rule: growth is the
  // only way to obtain a range, so the policy has one caller by construction.
  Store store(POOL);

  const Range first = *store.grow(Range{}, 0, 1, MAX_RANGE);
  REQUIRE(first.capacity == 4);

  const Range second = *store.grow(first, 4, 5, MAX_RANGE);
  REQUIRE(second.capacity == 8);

  const Range third = *store.grow(second, 8, 17, MAX_RANGE);
  REQUIRE(third.capacity == 32);
}

TEST_CASE("capacity is capped and a larger request is refused",
          "[rangestore]") {
  Store store(POOL);

  const std::optional<Range> capped =
    store.grow(Range{}, 0, MAX_RANGE, MAX_RANGE);

  REQUIRE(capped.has_value());
  REQUIRE(capped->capacity == MAX_RANGE);

  REQUIRE_FALSE(
    store.grow(Range{}, 0, MAX_RANGE + 1, MAX_RANGE).has_value()
  );
}

TEST_CASE("the pool refuses to grow past its capacity", "[rangestore]") {
  // Two full-sized ranges exhaust a 64 element pool, so the third request is
  // refused by the arena rather than by the per-range cap. Distinct failures:
  // the cap is covered by the case above.
  Store store(POOL);

  REQUIRE(store.grow(Range{}, 0, MAX_RANGE, MAX_RANGE).has_value());
  REQUIRE(store.grow(Range{}, 0, MAX_RANGE, MAX_RANGE).has_value());
  REQUIRE(store.size() == POOL);

  REQUIRE_FALSE(store.grow(Range{}, 0, 1, MAX_RANGE).has_value());
}
