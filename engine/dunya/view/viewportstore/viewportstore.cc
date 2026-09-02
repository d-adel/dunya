#include "viewportstore.ih"

namespace dunya::view {

ViewportId ViewportStore::create() {
  const auto free =
    std::find_if(m_slots.begin(), m_slots.end(), [](const auto& slot) {
      return !slot.has_value();
    });

  if (free != m_slots.end()) {
    free->emplace();

    return static_cast<ViewportId>(std::distance(m_slots.begin(), free));
  }

  m_slots.emplace_back(Viewport{});

  return static_cast<ViewportId>(m_slots.size() - 1);
}

bool ViewportStore::destroy(ViewportId id) {
  if (id >= m_slots.size() || !m_slots[id].has_value()) {
    return false;
  }

  m_slots[id].reset();

  return true;
}

bool ViewportStore::configure(ViewportId id, const Viewport& config) {
  if (id >= m_slots.size() || !m_slots[id].has_value()) {
    return false;
  }

  m_slots[id] = config;

  return true;
}

const Viewport* ViewportStore::find(ViewportId id) const {
  if (id >= m_slots.size() || !m_slots[id].has_value()) {
    return nullptr;
  }

  return &m_slots[id].value();
}

std::size_t ViewportStore::count() const noexcept {
  return static_cast<std::size_t>(
    std::count_if(m_slots.begin(), m_slots.end(), [](const auto& slot) {
      return slot.has_value();
    })
  );
}

void ViewportStore::clear() noexcept {
  m_slots.clear();
}

}
