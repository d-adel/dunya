#include "framecheck.ih"

namespace {

/* What counts as noise rather than change.
 *
 * Deliberately strict, because the run-to-run spread on this machine was
 * measured rather than guessed at: three runs of the same scene produced
 * bit-identical frames. A tolerance wider than the noise is a regression this
 * gate would wave through. It will need widening the day a driver update moves
 * the numbers - and *that* is the moment to measure again, not to raise it
 * until the red goes away.
 */
constexpr dunya::image::Tolerance TOLERANCE{0, 0, 255};

}  // namespace

FrameCheck::FrameCheck(
  const Context& context,
  const SwapChain& swapChain,
  const StartupOptions& options
)
    : m_context(context),
      m_swapChain(swapChain),
      m_screenshot(options.screenshot),
      m_reference(options.golden) {}

bool FrameCheck::wanted() const noexcept {
  return !m_screenshot.empty() || !m_reference.empty();
}

bool FrameCheck::ran() const noexcept {
  return m_ran;
}

bool FrameCheck::failed() const noexcept {
  return m_failed;
}

dunya::image::Bitmap FrameCheck::read(VkImage image) const {
  const VkExtent2D extent = m_swapChain.extent();

  // A minimised window presents nothing. Failing here is deliberate - a
  // zero-sized or stale capture silently blessed as a reference is the failure
  // the whole golden-image idea exists to prevent.
  if (extent.width == 0 || extent.height == 0) {
    throw std::runtime_error("Cannot capture a frame from a zero-sized window");
  }

  // The recorded frame left it ready to present, and it goes back that way.
  return dunya::capture::read(
    m_context.device(),
    image,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    extent,
    m_swapChain.imageFormat()
  );
}

/* A missing reference writes one and still fails.
 *
 * Passing instead would mean a mistyped path silently reports success forever,
 * which is the failure mode that makes a test worse than no test at all.
 */
bool FrameCheck::compareToReference(const dunya::image::Bitmap& frame) const {
  const std::filesystem::path reference(m_reference);

  if (!std::filesystem::exists(reference)) {
    dunya::image::save(frame, m_reference);

    std::cout << "golden " << m_reference
              << ": MISSING, wrote it. Look at it, then commit it.\n";

    return false;
  }

  const dunya::image::Bitmap expected = dunya::image::load(m_reference);

  const dunya::image::Difference difference =
    dunya::image::compare(expected, frame, TOLERANCE);

  if (dunya::image::passes(difference, TOLERANCE)) {
    std::cout << "golden " << m_reference << ": ok\n";
    return true;
  }

  std::cout << "golden " << m_reference << ": FAILED\n";

  if (!difference.comparable) {
    std::cout << "  size " << expected.width << "x" << expected.height
              << " became " << frame.width << "x" << frame.height << '\n';
    return false;
  }

  const uint64_t total =
    static_cast<uint64_t>(expected.width) * expected.height;

  std::cout << "  " << difference.differingPixels << " of " << total
            << " pixels differ, worst channel delta "
            << difference.worstChannelDelta << " at (" << difference.worstX
            << ", " << difference.worstY << ")\n";

  // Named after the reference but written to the working directory rather than
  // the source tree: the evidence belongs where the run happened.
  const std::string stem = reference.stem().string();

  dunya::image::save(frame, stem + "-actual.png");
  dunya::image::save(
    dunya::image::differenceImage(expected, frame, TOLERANCE),
    stem + "-diff.png"
  );

  std::cout << "  wrote " << stem << "-actual.png and " << stem
            << "-diff.png\n";

  return false;
}

void FrameCheck::run(VkImage image) {
  const dunya::image::Bitmap frame = read(image);

  if (!m_screenshot.empty()) {
    dunya::image::save(frame, m_screenshot);

    std::cout << "captured " << frame.width << "x" << frame.height << " to "
              << m_screenshot << '\n';
  }

  if (!m_reference.empty()) {
    m_failed = !compareToReference(frame);
  }

  m_ran = true;
}
