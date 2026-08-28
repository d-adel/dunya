#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dunya::image {

/* An 8-bit RGBA image in host memory.
 *
 * Always four channels, whatever the file happened to hold: a three-channel
 * reference compared against a four-channel capture is a comparison nobody
 * wants to reason about at the moment it fails.
 */
struct Bitmap {
  uint32_t width = 0;
  uint32_t height = 0;

  // RGBA, row major, no padding. width * height * 4 bytes.
  std::vector<uint8_t> pixels;
};

Bitmap load(const std::string& path);
void save(const Bitmap& bitmap, const std::string& path);

/* What two images differ by, with no opinion about whether that is acceptable.
 *
 * Two numbers rather than one, because there are two failure modes and either
 * number alone hides one of them. A single pixel gone wrong is invisible in a
 * count and obvious in worstChannelDelta; a faint shift across the whole frame
 * is obvious in the count while every individual pixel stays unremarkable.
 */
struct Difference {
  // False when the sizes disagree, in which case nothing below is meaningful.
  bool comparable = false;

  uint64_t differingPixels = 0;

  uint32_t worstChannelDelta = 0;
  uint32_t worstX = 0;
  uint32_t worstY = 0;
};

/* The policy half, kept apart from the measurement half.
 *
 * The real enemy of an image test is not a missed regression but a false alarm:
 * a test that cries wolf gets its diffs blessed without being read, and then it
 * produces confidence instead of information. Every field here exists to buy
 * that risk down.
 */
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

/* The reference dimmed to grey, with differing pixels marked in red.
 *
 * "Which pixels differ" and "by how much" are different pictures, and the
 * numbers already carry the magnitude. What the picture has to answer is
 * *where*, with enough of the scene left visible to recognise the part that
 * moved. Throws when the two are not comparable, because a difference image
 * between mismatched sizes would be a guess.
 */
Bitmap differenceImage(
  const Bitmap& reference,
  const Bitmap& candidate,
  const Tolerance& tolerance
);

}  // namespace dunya::image
