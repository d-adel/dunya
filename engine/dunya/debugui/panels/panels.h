#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dunya::debugui {

enum class WidgetKind : uint8_t {
  Value,
  Slider,
  Toggle,
  Button,
  Text,
  Separator
};

struct Widget {
  WidgetKind kind = WidgetKind::Value;

  std::string name;
  std::string unit;

  std::function<double()> read;
  std::function<void(double)> write;
  std::function<std::string()> text;
  std::function<void()> press;

  double minimum = 0.0;
  double maximum = 1.0;
};

class Panel {
public:
  explicit Panel(std::string name);

  [[nodiscard]] const std::string& name() const noexcept;

  [[nodiscard]] bool visible() const noexcept;
  void show(bool wanted) noexcept;

  [[nodiscard]] std::span<const Widget> widgets() const noexcept;

  Panel& value(
    std::string name,
    std::function<double()> read,
    std::string unit = {}
  );

  Panel& text(std::string name, std::function<std::string()> read);

  Panel& slider(
    std::string name,
    std::function<double()> read,
    std::function<void(double)> write,
    double minimum,
    double maximum,
    std::string unit = {}
  );

  Panel& toggle(
    std::string name,
    std::function<double()> read,
    std::function<void(double)> write
  );

  Panel& button(std::string name, std::function<void()> press);

  Panel& separator();

private:
  std::string m_name;
  bool m_visible = true;

  std::vector<Widget> m_widgets;
};

class Panels {
public:
  Panel& panel(std::string_view name);

  [[nodiscard]] Panel* find(std::string_view name) noexcept;

  [[nodiscard]] std::span<const Panel> panels() const noexcept;
  [[nodiscard]] std::span<Panel> panels() noexcept;

  [[nodiscard]] size_t size() const noexcept;

  void clear() noexcept;

private:
  std::vector<Panel> m_panels;
};

}
