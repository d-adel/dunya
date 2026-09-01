#include <catch2/catch_test_macros.hpp>

#include <capi/win32keys/win32keys.h>
#include <dunya/platform/keyevent/keyevent.h>
#include <dunya/systems/input/input.h>
#include <dunya/systems/schedule/schedule.h>

using dunya::systems::InputState;
using dunya::systems::Key;
using dunya::systems::MouseButton;

TEST_CASE("a key that goes down is held and pressed for one frame", "[input]") {
  InputState input;

  input.beginFrame();
  input.setKey(Key::F, true);

  REQUIRE(input.held(Key::F));
  REQUIRE(input.pressed(Key::F));
  REQUIRE_FALSE(input.released(Key::F));

  input.beginFrame();

  REQUIRE(input.held(Key::F));
  REQUIRE_FALSE(input.pressed(Key::F));
}

TEST_CASE("a key that goes up is released for one frame", "[input]") {
  InputState input;

  input.setKey(Key::G, true);
  input.beginFrame();
  input.setKey(Key::G, false);

  REQUIRE_FALSE(input.held(Key::G));
  REQUIRE(input.released(Key::G));

  input.beginFrame();

  REQUIRE_FALSE(input.released(Key::G));
}

TEST_CASE("keys do not bleed into one another", "[input]") {
  InputState input;

  input.setKey(Key::A, true);

  REQUIRE(input.held(Key::A));
  REQUIRE_FALSE(input.held(Key::B));
  REQUIRE_FALSE(input.held(Key::Space));
  REQUIRE_FALSE(input.held(Key::F5));
}

TEST_CASE("mouse buttons carry the same edges as keys", "[input]") {
  InputState input;

  input.beginFrame();
  input.setMouseButton(MouseButton::Left, true);

  REQUIRE(input.held(MouseButton::Left));
  REQUIRE(input.pressed(MouseButton::Left));
  REQUIRE_FALSE(input.held(MouseButton::Right));

  input.beginFrame();

  REQUIRE_FALSE(input.pressed(MouseButton::Left));
}

TEST_CASE("the cursor is reported where the host put it", "[input]") {
  InputState input;

  input.setCursor(320.0f, 240.0f);

  REQUIRE(input.cursorX() == 320.0f);
  REQUIRE(input.cursorY() == 240.0f);
}

TEST_CASE("an unknown key is ignored rather than written", "[input]") {
  InputState input;

  input.setKey(Key::Unknown, true);
  input.setKey(Key::Count, true);

  REQUIRE_FALSE(input.held(Key::Unknown));
}

TEST_CASE("a system reads the input the host filled", "[input]") {
  dunya::objectmodel::World world;

  InputState input;

  input.setKey(Key::F, true);

  dunya::systems::Schedule schedule;

  bool sawTheKey = false;

  REQUIRE(schedule.add(0, "reads input", [&sawTheKey](auto& context) {
    sawTheKey = context.input.held(Key::F);
  }));

  dunya::systems::Context context{world, input, 0.016f, 1u};

  schedule.run(context);

  REQUIRE(sawTheKey);
}

TEST_CASE("the win32 mapping lands on the key it names", "[input]") {
  REQUIRE(dunya::capi::keyFromWin32(0x41) == Key::A);
  REQUIRE(dunya::capi::keyFromWin32(0x5A) == Key::Z);
  REQUIRE(dunya::capi::keyFromWin32(0x30) == Key::Digit0);
  REQUIRE(dunya::capi::keyFromWin32(0x39) == Key::Digit9);
  REQUIRE(dunya::capi::keyFromWin32(0x70) == Key::F1);
  REQUIRE(dunya::capi::keyFromWin32(0x74) == Key::F5);
  REQUIRE(dunya::capi::keyFromWin32(0x7B) == Key::F12);
  REQUIRE(dunya::capi::keyFromWin32(0x20) == Key::Space);
  REQUIRE(dunya::capi::keyFromWin32(0x1B) == Key::Escape);
  REQUIRE(dunya::capi::keyFromWin32(0x25) == Key::Left);
  REQUIRE(dunya::capi::keyFromWin32(0x26) == Key::Up);
  REQUIRE(dunya::capi::keyFromWin32(0x27) == Key::Right);
  REQUIRE(dunya::capi::keyFromWin32(0x28) == Key::Down);
  REQUIRE(dunya::capi::keyFromWin32(0x00) == Key::Unknown);
}

TEST_CASE("only a physical transition moves a key", "[input]") {
  using dunya::platform::KeyEventType;
  using dunya::platform::keyTransition;

  REQUIRE(keyTransition(KeyEventType::Pressed) == true);
  REQUIRE(keyTransition(KeyEventType::Released) == false);

  REQUIRE_FALSE(keyTransition(KeyEventType::SinglePressed).has_value());
  REQUIRE_FALSE(keyTransition(KeyEventType::DoublePressed).has_value());
  REQUIRE_FALSE(keyTransition(KeyEventType::Hold).has_value());
  REQUIRE_FALSE(keyTransition(KeyEventType::Repeat).has_value());
}

TEST_CASE("a gesture after a release fires no second edge", "[input]") {
  using dunya::platform::KeyEventType;
  using dunya::platform::keyTransition;

  InputState input;

  const KeyEventType stream[]{
    KeyEventType::Pressed,
    KeyEventType::Hold,
    KeyEventType::Released,
    KeyEventType::SinglePressed
  };

  uint32_t edges = 0u;

  for (KeyEventType type : stream) {
    input.beginFrame();

    if (const std::optional<bool> down = keyTransition(type)) {
      input.setKey(Key::F, *down);
    }

    if (input.pressed(Key::F)) {
      ++edges;
    }
  }

  REQUIRE(edges == 1u);
  REQUIRE_FALSE(input.held(Key::F));
}

TEST_CASE(
  "a tap that starts and ends inside one frame is still a press",
  "[input]"
) {
  InputState input;

  input.beginFrame();

  input.setMouseButton(MouseButton::Left, true);
  input.setMouseButton(MouseButton::Left, false);

  REQUIRE(input.pressed(MouseButton::Left));
  REQUIRE(input.released(MouseButton::Left));
  REQUIRE_FALSE(input.held(MouseButton::Left));

  input.beginFrame();

  REQUIRE_FALSE(input.pressed(MouseButton::Left));
  REQUIRE_FALSE(input.released(MouseButton::Left));
}

TEST_CASE("a key repeated down without a release fires one press", "[input]") {
  InputState input;

  input.beginFrame();

  input.setKey(Key::F, true);
  input.setKey(Key::F, true);
  input.setKey(Key::F, true);

  REQUIRE(input.pressed(Key::F));

  input.beginFrame();

  REQUIRE(input.held(Key::F));
  REQUIRE_FALSE(input.pressed(Key::F));
}

TEST_CASE("every tap between two frames is counted", "[input]") {
  InputState input;

  uint32_t seen = 0u;

  for (uint32_t frame = 0u; frame != 5u; ++frame) {
    input.beginFrame();

    input.setKey(Key::F, true);
    input.setKey(Key::F, false);

    if (input.pressed(Key::F)) {
      ++seen;
    }
  }

  REQUIRE(seen == 5u);
}
