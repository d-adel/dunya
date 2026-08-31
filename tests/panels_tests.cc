#include <catch2/catch_test_macros.hpp>

#include <dunya/debugui/panels/panels.h>

using dunya::debugui::Panel;
using dunya::debugui::Panels;
using dunya::debugui::WidgetKind;

TEST_CASE("naming a panel twice returns the same one", "[panels]") {
  Panels registry;

  registry.panel("Physics").value("bodies", [] { return 3.0; });
  registry.panel("Physics").value("contacts", [] { return 7.0; });

  REQUIRE(registry.size() == 1);
  REQUIRE(registry.panel("Physics").widgets().size() == 2);
}

TEST_CASE("a widget reads through its binding, not a copy", "[panels]") {
  Panels registry;

  double live = 1.0;

  registry.panel("World").value("entities", [&live] { return live; });

  const auto& widget = registry.panel("World").widgets()[0];

  REQUIRE(widget.read() == 1.0);

  live = 42.0;

  REQUIRE(widget.read() == 42.0);
}

TEST_CASE("a slider writes back through its binding", "[panels]") {
  Panels registry;

  double gravity = 9.81;

  registry.panel("Physics").slider(
    "gravity",
    [&gravity] { return gravity; },
    [&gravity](double set) { gravity = set; },
    0.0,
    20.0,
    "m/s2"
  );

  const auto& widget = registry.panel("Physics").widgets()[0];

  REQUIRE(widget.kind == WidgetKind::Slider);
  REQUIRE(widget.minimum == 0.0);
  REQUIRE(widget.maximum == 20.0);
  REQUIRE(widget.unit == "m/s2");

  widget.write(3.5);

  REQUIRE(gravity == 3.5);
}

TEST_CASE("a button runs its action when pressed", "[panels]") {
  Panels registry;

  int pressed = 0;

  registry.panel("Shot").button("fire", [&pressed] { ++pressed; });

  registry.panel("Shot").widgets()[0].press();
  registry.panel("Shot").widgets()[0].press();

  REQUIRE(pressed == 2);
}

TEST_CASE("a panel can be curated out of a build", "[panels]") {
  Panels registry;

  registry.panel("March").value("steps", [] { return 1.0; });

  REQUIRE(registry.panel("March").visible());

  registry.panel("March").show(false);

  REQUIRE(!registry.panel("March").visible());
  REQUIRE(registry.find("March") != nullptr);
  REQUIRE(registry.find("Absent") == nullptr);
}

TEST_CASE("every widget describes itself without being drawn", "[panels]") {
  Panels registry;

  bool shadows = true;

  registry.panel("Render")
    .value(
      "draws",
      [] { return 12.0; },
      "calls"
    )
    .separator()
    .toggle(
      "shadows",
      [&shadows] { return shadows ? 1.0 : 0.0; },
      [&shadows](double set) { shadows = set != 0.0; }
    )
    .text("device", [] { return std::string("RTX 4060"); });

  const auto widgets = registry.panel("Render").widgets();

  REQUIRE(widgets.size() == 4);
  REQUIRE(widgets[0].kind == WidgetKind::Value);
  REQUIRE(widgets[0].unit == "calls");
  REQUIRE(widgets[1].kind == WidgetKind::Separator);
  REQUIRE(widgets[2].kind == WidgetKind::Toggle);
  REQUIRE(widgets[3].kind == WidgetKind::Text);
  REQUIRE(widgets[3].text() == "RTX 4060");

  widgets[2].write(0.0);

  REQUIRE(!shadows);
}

TEST_CASE("clearing drops every panel", "[panels]") {
  Panels registry;

  registry.panel("One").value("a", [] { return 0.0; });
  registry.panel("Two").value("b", [] { return 0.0; });

  REQUIRE(registry.size() == 2);

  registry.clear();

  REQUIRE(registry.size() == 0);
  REQUIRE(registry.panels().empty());
}
