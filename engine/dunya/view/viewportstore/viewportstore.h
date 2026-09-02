#pragma once

#include <dunya/view/viewport/viewport.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace dunya::view {

class ViewportStore {
public:
  ViewportStore() = default;

  ViewportStore(const ViewportStore&) = delete;
  ViewportStore& operator=(const ViewportStore&) = delete;

  [[nodiscard]] ViewportId create();

  [[nodiscard]] bool destroy(ViewportId id);

  [[nodiscard]] bool configure(ViewportId id, const Viewport& config);

  [[nodiscard]] const Viewport* find(ViewportId id) const;

  [[nodiscard]] std::size_t count() const noexcept;

  void clear() noexcept;

private:
  std::vector<std::optional<Viewport>> m_slots;
};

}
