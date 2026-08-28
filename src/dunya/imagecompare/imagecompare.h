#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dunya::image {

// An 8-bit RGBA image in host memory. Always four channels, so a three-channel
// reference never meets a four-channel capture.
struct Bitmap {
  uint32_t width = 0;
  uint32_t height = 0;

  // RGBA, row major, no padding. width * height * 4 bytes.
  std::vector<uint8_t> pixels;
};

Bitmap load(const std::string& path);
void save(const Bitmap& bitmap, const std::string& path);

// What two images differ by, with no opinion on whether it is acceptable. Two
// numbers because either alone hides a failure mode.
struct Difference {
  // False when the sizes disagree, in which case nothing below is meaningful.
  bool comparable = false;

  uint64_t differingPixels = 0;

  uint32_t worstChannelDelta = 0;
  uint32_t worstX = 0;
  uint32_t worstY = 0;
};

// The policy half, apart from the measurement half. The enemy of an image test
// is the false alarm: one that cries wolf gets blessed unread.
struct Tolerance {
  // Per-channel delta treated as noise rather than as change.
  uint32_t channelDelta = 0;

  // How many pixels may exceed it before the image counts as changed.
  uint64_t allowedPixels = 0;

  // A delta this large fails whatever the count says, so one catastrophic
  // pixel cannot hide inside an allowance meant for faint drift.
  uint32_t fatalChannelDelta = 255;
};

// Measures. The tolerance is needed to know what counts as noise, but no
// verdict is reached here - a passing comparison still reports its numbers.
Difference compare(
  const Bitmap& reference,
  const Bitmap& candidate,
  const Tolerance& tolerance
);

[[nodiscard]]
bool passes(const Difference& difference, const Tolerance& tolerance) noexcept;

// The reference dimmed to grey, differing pixels in red: the numbers carry the
// magnitude, the picture answers where. Throws if the sizes disagree.
Bitmap differenceImage(
  const Bitmap& reference,
  const Bitmap& candidate,
  const Tolerance& tolerance
);

}  // namespace dunya::image
