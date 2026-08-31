#include "panels.ih"

namespace dunya::debugui {

Panel::Panel(std::string name) : m_name(std::move(name)) {}

const std::string& Panel::name() const noexcept {
  return m_name;
}

bool Panel::visible() const noexcept {
  return m_visible;
}

void Panel::show(bool wanted) noexcept {
  m_visible = wanted;
}

std::span<const Widget> Panel::widgets() const noexcept {
  return m_widgets;
}

Panel& Panel::value(
  std::string name,
  std::function<double()> read,
  std::string unit
) {
  Widget widget{};

  widget.kind = WidgetKind::Value;
  widget.name = std::move(name);
  widget.unit = std::move(unit);
  widget.read = std::move(read);

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panel::text(std::string name, std::function<std::string()> read) {
  Widget widget{};

  widget.kind = WidgetKind::Text;
  widget.name = std::move(name);
  widget.text = std::move(read);

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panel::slider(
  std::string name,
  std::function<double()> read,
  std::function<void(double)> write,
  double minimum,
  double maximum,
  std::string unit
) {
  Widget widget{};

  widget.kind = WidgetKind::Slider;
  widget.name = std::move(name);
  widget.unit = std::move(unit);
  widget.read = std::move(read);
  widget.write = std::move(write);
  widget.minimum = minimum;
  widget.maximum = maximum;

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panel::toggle(
  std::string name,
  std::function<double()> read,
  std::function<void(double)> write
) {
  Widget widget{};

  widget.kind = WidgetKind::Toggle;
  widget.name = std::move(name);
  widget.read = std::move(read);
  widget.write = std::move(write);

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panel::button(std::string name, std::function<void()> press) {
  Widget widget{};

  widget.kind = WidgetKind::Button;
  widget.name = std::move(name);
  widget.press = std::move(press);

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panel::separator() {
  Widget widget{};

  widget.kind = WidgetKind::Separator;

  m_widgets.push_back(std::move(widget));

  return *this;
}

Panel& Panels::panel(std::string_view name) {
  if (Panel* found = find(name)) {
    return *found;
  }

  m_panels.emplace_back(std::string(name));

  return m_panels.back();
}

Panel* Panels::find(std::string_view name) noexcept {
  for (Panel& panel : m_panels) {
    if (panel.name() == name) {
      return &panel;
    }
  }

  return nullptr;
}

std::span<const Panel> Panels::panels() const noexcept {
  return m_panels;
}

std::span<Panel> Panels::panels() noexcept {
  return m_panels;
}

size_t Panels::size() const noexcept {
  return m_panels.size();
}

void Panels::clear() noexcept {
  m_panels.clear();
}

}
