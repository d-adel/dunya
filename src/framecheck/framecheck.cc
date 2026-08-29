#include "framecheck.ih"

namespace {

// What counts as noise rather than change. Strict, because three runs here
// produced bit-identical frames; widen it by measuring, not to clear red.
constexpr dunya::image::Tolerance TOLERANCE{0, 0, 255};

}  // namespace

FrameCheck::FrameCheck(
  const dunya::gpu::Context& context,
  const dunya::gpu::SwapChain& swapChain,
  const StartupOptions& options
)
    : m_context(context),
      m_swapChain(swapChain),
      m_capture(options.capture),
      m_screenshot(options.screenshot),
      m_reference(options.golden) {}

bool FrameCheck::wanted() const noexcept {
  return !m_screenshot.empty() || !m_reference.empty();
}

FrameCheck::~FrameCheck() {
  if (!m_stream.is_open()) {
    return;
  }

  m_stream.close();

  // The stream carries no header, so the size it was written at has to reach
  // whoever encodes it. Printed rather than stored, because the next step is a
  // person running ffmpeg.
  std::cout << "Recorded " << m_captured << " frames of " << m_width << "x"
            << m_height << " to " << m_capture << "/frames.raw\n"
            << "  ffmpeg -f rawvideo -pixel_format rgba -video_size " << m_width
            << "x" << m_height << " -framerate 60 -i " << m_capture
            << "/frames.raw ...\n";
}

double FrameCheck::lastCaptureMs() const noexcept {
  return m_lastCaptureMs;
}

bool FrameCheck::capturing() const noexcept {
  return !m_capture.empty();
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

// A missing reference writes one and still fails; passing would let a mistyped
// path report success forever.
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
  const auto started = std::chrono::steady_clock::now();

  const dunya::image::Bitmap frame = read(image);

  // A recording rather than a test: it does not set m_ran, so the loop keeps
  // going.
  //
  // One raw stream rather than a PNG apiece. Compressing a 3.7 MB frame sixty
  // times a second costs far more than drawing it - it ran the recording at
  // twenty-odd frames a second, which looks like the engine failing and is
  // only the encoder. Raw is a memcpy to a sequential file; ffmpeg reads it
  // directly and does the compressing afterwards, when nothing is waiting.
  if (!m_capture.empty()) {
    if (!m_stream.is_open()) {
      m_stream.open(m_capture + "/frames.raw", std::ios::binary);

      if (!m_stream) {
        throw std::runtime_error("Cannot write the recording to " + m_capture);
      }

      m_width = frame.width;
      m_height = frame.height;
    }

    m_stream.write(
      reinterpret_cast<const char*>(frame.pixels.data()),
      static_cast<std::streamsize>(frame.pixels.size())
    );

    ++m_captured;

    m_lastCaptureMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started
    )
                        .count();

    return;
  }

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
