#include "application.ih"

namespace {

// Five is three frames of headroom at 60 Hz and still bounds the worst case.
constexpr uint32_t MAX_PHYSICS_SUBSTEPS = 5;

// Balls alive at once. Each one holds a pool slot and a body, so the cap is
// what keeps a held-down key from exhausting either.
constexpr size_t MAX_BALLS = 24;

// How far in front of the camera a ball appears, in metres. Inside the near
// plane it would fill the screen on the frame it is fired.
constexpr float MUZZLE_DISTANCE = 2.5f;

const std::vector<VkVertexInputBindingDescription> MESH_BINDINGS{
  dunya::renderer::Vertex::getBindingDescription()
};

const auto MESH_ATTRIBUTES =
  dunya::renderer::Vertex::getAttributeDescriptions();

}  // namespace

Application::Application(const StartupOptions& options)
    : m_input(m_context.window().handle()),
      m_swapChain(m_context),
      m_scene(
        m_context,
        m_authoredWorld,
        options.wallColumns,
        options.wallRows,
        options.wallDepth
      ),
      m_uploader(m_context.device()),
      m_frameGlobals(m_context.device()),
      m_resourceTable(
        m_context.device(),
        m_scene.textures(),
        m_scene.samplers(),
        m_scene.materials()
      ),
      m_recordTable(m_context.device()),
      m_fieldBaker(m_context.device(), m_recordTable),
      m_volumePool(m_context.device()),
      m_meshPipeline(
        dunya::gpu::PipelineType::Mesh,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{

          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout()
        },
        m_swapChain,
        MESH_BINDINGS,
        MESH_ATTRIBUTES
      ),
      m_fieldPipeline(
        dunya::gpu::PipelineType::Field,
        m_context.device().vkDevice(),
        std::vector<VkDescriptorSetLayout>{
          m_frameGlobals.setLayout(),
          m_resourceTable.setLayout(),
          m_recordTable.setLayout()
        },
        m_swapChain
      ),
      m_renderer(
        m_context.device(),
        m_recordTable,
        m_fieldBaker,
        m_volumePool,
        m_frameGlobals,
        m_meshPipeline,
        m_fieldPipeline,
        m_resourceTable,
        m_context.surface().handle(),
        m_swapChain.imageCount()
      ),
      m_overlay(m_context, m_swapChain),
      m_fieldEditor(m_authoredWorld),
      m_cameraInput({}),
      m_prevAcceptsInput(false),
      m_reloadRequested(false)

{
  m_volumeOwners.fill(dunya::objectmodel::INVALID_ENTITY);

  m_keySubscription = dunya::core::EventDispatcher::instance()
                        .subscribe<dunya::platform::KeyEvent>(
                          [this](const dunya::platform::KeyEvent& event) {
                            handleKeyEvent(event);
                          }
                        );

  m_mouseSubscription =
    dunya::core::EventDispatcher::instance()
      .subscribe<dunya::platform::MouseButtonEvent>(
        [this](const dunya::platform::MouseButtonEvent& event) {
          handleMouseButtonEvent(event);
        }
      );
}

Application::~Application() {
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::MouseButtonEvent>(m_mouseSubscription);
  dunya::core::EventDispatcher::instance()
    .unsubscribe<dunya::platform::KeyEvent>(m_keySubscription);
}

int Application::start(const StartupOptions& options) {
  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    GLFW_CURSOR_NORMAL
  );

  std::cout << "Click to fire at the wall, F to fire at its middle\n"
            << "Hold right mouse to look and fly (WASD/QE)\n"
            << "G resets the wall, F5 stops the simulation\n"
            << "Alt + click carves by hand once stopped\n";

  if (options.carves > 0) {
    m_fieldEditor.stress(options.carves);
  }

  // Not applied here: the deformable has no lattice until the frame loop bakes
  // it, so the dents wait for the frame that gives it one.
  m_pendingDents = options.dents;
  m_dentLogPath = options.dentLog;

  m_demoFrames = options.demo;

  if (options.demoRate > 0.0f) {
    m_demoInterval = std::max(1u, uint32_t(60.0f / options.demoRate + 0.5f));
  }

  if (options.analytic) {
    m_frameContext.fieldRepresentation = dunya::core::FIELD_ANALYTIC;
    std::cout << "Field representation: analytic\n";
  }

  m_scene.frame(m_camera);

  m_shotSettings = m_scene.projectile();

  // Built here rather than per shot, on the field the scene owns rather than
  // on a copy: every ball is the same ball, and a Jolt shape is immutable and
  // refcounted, so one is what they all want.
  m_ballShape = new dunya::physics::FieldShape(m_scene.projectileField());

  // Asked for now, while a loading notice is up, because the answer is kept:
  // the first shot would otherwise pay the walk that every later one skips.
  static_cast<void>(m_ballShape->GetMassProperties());

  m_ballVolume = m_volumePool.allocate(m_scene.projectileField());

  if (m_ballVolume != UINT32_MAX) {
    const auto ballImages = m_volumePool.images(m_ballVolume);

    m_recordTable.registerVolume(
      ballImages.distance.imageView(),
      ballImages.material.imageView(),
      m_ballVolume
    );

    // Once, here, for every ball there will ever be. The slot is keyed on the
    // volume index and every ball shares this one volume, so the bounds the
    // shading and shadow march read are as constant as the geometry is. This
    // is what lets fire() skip the bake pass entirely.
    m_recordTable.uploadBounds(m_ballVolume, m_scene.projectileField());
  }

  bool bakeCheckPending = options.verifyBake;
  bool tableFullReported = false;
  bool volumePoolFullReported = false;

  FrameCheck frameCheck(m_context, m_swapChain, options);

  // After the FrameCheck, because what a recording shows is not what an
  // interactive session shows.
  registerPanels();

  // Not in a capture run: the deferral would make the notice the first frame
  // presented, and that is the frame a golden compares.
  if (!frameCheck.wanted()) {
    announce("Baking fields", Transition::None);
  }

  std::function<void(VkImage)> captureHook;

  if (frameCheck.wanted() || frameCheck.capturing()) {
    captureHook = [&frameCheck](VkImage image) {
      frameCheck.run(image);
    };
  }

  if (frameCheck.capturing()) {
    std::filesystem::create_directories(options.capture);

    std::cout << "Recording every frame to " << options.capture << '\n';
  }

  double prevTime = glfwGetTime();
  double pipelineReloadCheck = 0;
  double statWindowStart = prevTime;
  double physicsAccumulator = 0.0;
  uint32_t statFrames = 0;

  // Seconds this stat window spent recording rather than rendering.
  double statCaptureSeconds = 0.0;

  while (!glfwWindowShouldClose(m_context.window().handle())) {
    double now = glfwGetTime();

    // What the frame actually took, kept separately from what the simulation
    // is told, because a capture run lies to the simulation on purpose.
    const float realDt = static_cast<float>(now - prevTime);

    float dt = realDt;

    prevTime = now;

    // Reading every frame back and writing a PNG takes far longer than
    // drawing it, so a recording paced by the wall clock would simulate in
    // slow motion and play back as a stutter. A fixed step makes the frames a
    // film: sixty of them is one second of simulation, whatever the encoder
    // was doing.
    //
    // It follows that a capture run measures nothing. The frame times below
    // are the real ones, and a capture run's are dominated by the readback -
    // the performance claim has to come from a run without --capture.
    if (frameCheck.capturing()) {
      dt = 1.0f / 60.0f;
    }

    ++m_frameIndex;

    // Frees the staging the GPU has finished reading. Asks rather than waits,
    // so it costs nothing on the frames - most of them - with nothing to free.
    m_uploader.retire();

    // The app is the demo, so it starts running rather than waiting for F5.
    // The wall has no bodies until its fields have been baked, which is the
    // frame loop's job, so this cannot happen in start().
    //
    // Two runs opt out. A capture compares the first presented frame, and that
    // frame is the authored scene. And the dent harness measures deformation
    // alone: a simulation running underneath it would put a collapsing wall in
    // every row.
    constexpr uint32_t DEMO_PLAY_FRAME = 4u;

    const bool measuring = frameCheck.wanted() || m_pendingDents > 0;

    if (m_frameIndex == DEMO_PLAY_FRAME && !measuring && !m_runtime) {
      play();
    }

    // A run nobody is watching, so the schedule is frames rather than
    // seconds: the same shot lands on the same frame whatever the machine
    // does, and a ball goes every four seconds of simulated time.
    if (m_demoFrames > 0) {
      // Long enough for the wall to settle out of its drop, short enough that
      // a ten-second recording is not a quarter empty.
      constexpr uint32_t DEMO_FIRST_SHOT = 80u;

      const uint32_t DEMO_SHOT_INTERVAL = m_demoInterval;

      const bool fired =
        m_frameIndex >= DEMO_FIRST_SHOT
        && (m_frameIndex - DEMO_FIRST_SHOT) % DEMO_SHOT_INTERVAL == 0u;

      if (fired) {
        // Spread across the wall rather than all at one spot, by the same R2
        // sequence the dents use: successive multiples of these two fractions
        // fill a square evenly without ever repeating, so a hundred boxes all
        // get hit and the same run does it the same way twice.
        const glm::vec2 at = glm::fract(
          static_cast<float>(m_shotsFired) * glm::vec2(0.7548777f, 0.5698403f)
        );

        ++m_shotsFired;

        fire(aimAtPoint(m_scene.wallPoint(at.x, at.y)));
      }

      // Frame times are the acceptance, so they are collected rather than
      // watched. The first few are excluded: they carry the bake of every
      // object in the scene and say nothing about steady state.
      constexpr uint32_t DEMO_WARMUP = 20u;

      // dt and the two phase timers all describe the frame that just ended,
      // so they are recorded together and against its index rather than this
      // one's.
      if (m_frameIndex > DEMO_WARMUP) {
        m_demoFrameMs.push_back(
          {m_frameIndex - 1u,
           realDt * 1000.0f,
           m_cratersApplied - m_cratersReported,
           fired,
           m_frameCarveMs,
           m_frameUploadMs,
           m_framePhysicsMs}
        );
      }

      m_cratersReported = m_cratersApplied;
      m_frameCarveMs = 0.0f;
      m_frameUploadMs = 0.0f;
      m_framePhysicsMs = 0.0f;

      if (m_frameIndex >= m_demoFrames) {
        reportDemo();
        glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
      }
    }

    // E5: no runtime, no physics. The accumulator only advances while a
    // runtime exists, so Play does not begin by discharging a backlog.
    if (m_runtime) {
      physicsAccumulator += dt;

      // A slow frame must not buy more steps than the next frame can afford,
      // or the loop feeds itself: more steps, slower frame, more steps.
      uint32_t substeps = 0;

      const auto stepStart = std::chrono::steady_clock::now();

      while (physicsAccumulator >= dunya::physics::PhysicsWorld::TIME_STEP
             && substeps != MAX_PHYSICS_SUBSTEPS) {
        ++substeps;
        m_runtime->step();

        physicsAccumulator -= dunya::physics::PhysicsWorld::TIME_STEP;
      }

      m_framePhysicsMs = std::chrono::duration<float, std::milli>(
                           std::chrono::steady_clock::now() - stepStart
      )
                           .count();

      // Whatever is left would be paid for next frame and start the spiral
      // again, so it is dropped: simulation time lags, frame rate does not.
      if (substeps == MAX_PHYSICS_SUBSTEPS) {
        physicsAccumulator = 0.0;
      }

      // D3: after the solve, never inside it. The contacts of every substep
      // are drained together, so a frame that stepped four times craters once
      // per impact rather than four times.
      {
        const auto carveStart = std::chrono::steady_clock::now();

        applyImpacts();

        m_frameCarveMs = std::chrono::duration<float, std::milli>(
                           std::chrono::steady_clock::now() - carveStart
        )
                           .count();
      }

      // Once per frame, not once per step: the world only needs where things
      // ended up, and the renderer reads it straight after.
      m_runtime->syncPoses();
    }

    ++statFrames;
    statCaptureSeconds += frameCheck.lastCaptureMs() / 1000.0;
    if (now - statWindowStart >= 1.0) {
      // The recording's own readback and PNG encode come out first. They are
      // not what the engine costs, and the panel they end up on is the point
      // of the recording.
      const double elapsed = now - statWindowStart - statCaptureSeconds;

      const double msPerFrame = (elapsed * 1000.0) / statFrames;
      m_lastFrameMs = msPerFrame;

      statCaptureSeconds = 0.0;

      std::cout << modeName(m_frameContext.mode) << "  "
                << m_swapChain.extent().width << "x"
                << m_swapChain.extent().height << "  " << std::fixed
                << std::setprecision(2) << msPerFrame << " ms  "
                << std::setprecision(0) << (statFrames / elapsed)
                << " fps  balls " << m_balls.size()
                << "\n"
                // Both stick, so every float printed after the first second
                // would otherwise come out rounded to a whole number.
                << std::defaultfloat << std::setprecision(6);

      statWindowStart = now;
      statFrames = 0;
    }

    glfwPollEvents();

    bool checkReady = now - pipelineReloadCheck > 0.5;

    if (checkReady) {
      pipelineReloadCheck = now;
    }

    if (m_reloadRequested || checkReady) {
      if (m_reloadRequested) {
        m_reloadRequested = false;
        m_context.device().waitIdle();
        m_meshPipeline.reload();
        m_fieldPipeline.reload();
      } else {
        if (m_meshPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_meshPipeline.reload();
        }
        if (m_fieldPipeline.sourcesChanged()) {
          m_context.device().waitIdle();
          m_fieldPipeline.reload();
        }
      }
    }

    bool curAcceptsInput = acceptsInput() && m_looking;

    if (curAcceptsInput != m_prevAcceptsInput) {
      m_input.cursorDeltaInvalid();
      m_prevAcceptsInput = curAcceptsInput;
    }

    m_input.update();

    if (curAcceptsInput) {
      m_cameraInput.lookDx = static_cast<float>(m_input.cursor().dx);
      m_cameraInput.lookDy = static_cast<float>(m_input.cursor().dy);
      m_camera.update(dt, m_cameraInput);
    }

    float aspect = static_cast<float>(m_swapChain.extent().width)
                   / static_cast<float>(m_swapChain.extent().height);

    m_recordTable.newFrame();
    // ---------- Frame context ----------
    m_frameContext.proj = m_camera.projectionMatrix(aspect);
    m_frameContext.view = m_camera.viewMatrix();
    m_frameContext.cameraPos = m_camera.position();

    // One frame of notice before a stall, and the work on the frame after: the
    // image left on screen while the loop blocks is then the message rather
    // than the picture from before the key was pressed.
    const bool announcing = m_stall == Stall::Announced;

    if (announcing) {
      m_stall = Stall::Working;
    } else if (m_stall == Stall::Working) {
      if (m_transition == Transition::ToRuntime) {
        play();
      } else if (m_transition == Transition::ToAuthoring) {
        stop();
      } else if (m_transition == Transition::Restart) {
        // Through Stop and Play rather than by putting every body back: the
        // runtime world is built from the authored one, so rebuilding it is
        // the reset, and both halves are already proven.
        stop();
        play();
      }

      m_transition = Transition::None;
      m_stall = Stall::None;
      m_overlay.notice("");
    }

    dunya::objectmodel::World& world = activeWorld();
    const entt::registry& registry = world.registry();

    // Reclaim slots whose entity is gone, before anything asks for a new one.
    // Done here rather than by a listener inside VolumePool, because the pool
    // is a renderer resource and the registry is world state: the frame is
    // where the two meet.
    for (uint32_t slot = 0; slot != m_volumeOwners.size(); ++slot) {
      const dunya::objectmodel::Entity owner = m_volumeOwners[slot];

      if (owner == dunya::objectmodel::INVALID_ENTITY) {
        continue;
      }

      if (
        registry.valid(owner)
        && registry.all_of<dunya::objectmodel::BakedVolume>(owner)
      ) {
        continue;
      }

      m_volumePool.release(slot);
      m_volumeOwners[slot] = dunya::objectmodel::INVALID_ENTITY;
    }

    uint32_t recordIndex = 0;

    m_recordEntities.clear();

    // The notice frame presents before the work rather than with it, so it
    // visits nothing and the records it leaves are last frame's.
    const std::span<const dunya::objectmodel::Entity> visiting =
      announcing ? std::span<const dunya::objectmodel::Entity>()
                 : world.fields();

    for (const dunya::objectmodel::Entity entity : visiting) {
      // The GPU record table holds MAX_FIELD_RECORDS.
      // The world has no such limit, so the frame is where the two meet.
      if (recordIndex == dunya::core::MAX_FIELD_RECORDS) {
        if (!tableFullReported) {
          tableFullReported = true;
          std::cout << "Field record table full, the rest are not drawn\n";
        }

        break;
      }

      const dunya::objectmodel::SdfGrid& grid =
        registry.get<dunya::objectmodel::SdfGrid>(entity);

      // No BakedVolume means no pool slot yet. Absence is the state; there is
      // no sentinel to get wrong.

      if (!registry.all_of<dunya::objectmodel::BakedVolume>(entity)) {
        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);

        const auto* carried =
          registry.try_get<dunya::field::SampledField>(entity);

        // A bake is seconds and a copy is milliseconds. The field the entity
        // already carries was made from these same primitives, so a volume can
        // be filled from it rather than from a second identical bake.
        const bool reusable =
          carried != nullptr && !world.needsResample(entity);

        dunya::field::SampledField baked;

        if (!reusable) {
          const dunya::field::Aabb box =
            dunya::objectmodel::gridBox(primitives);

          baked = dunya::field::bake(
            primitives,
            box.minimum,
            box.maximum,
            grid.resolution
          );
        }

        const uint32_t index =
          m_volumePool.allocate(reusable ? *carried : baked);

        // No volume means nothing to sample, and UINT32_MAX would index
        // the volume array out of bounds on the GPU. Skip it entirely.
        if (index == UINT32_MAX) {
          if (!volumePoolFullReported) {
            volumePoolFullReported = true;
            std::cout << "Volume pool full, object not drawn\n";
          }

          continue;
        }

        auto images = m_volumePool.images(index);

        m_recordTable.registerVolume(
          images.distance.imageView(),
          images.material.imageView(),
          index
        );

        m_volumeOwners[index] = entity;

        world.setBakedVolume(entity, index);

        // Physics queries this, so a fresh bake is kept rather than dropped
        // once the volume is uploaded. A reused one is already in place.
        if (!reusable) {
          world.setSampledField(entity, std::move(baked));
        }

        // After the field is in place, not before: when it is not reusable
        // the component does not exist until the line above. A deformable
        // never joins the bake list, and that dispatch is what fills the
        // bound table, so without this its slot holds whatever the
        // device-local buffer came up with and the march reads noise.
        if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
          m_recordTable.uploadBounds(
            index,
            registry.get<dunya::field::SampledField>(entity)
          );
        }

        // The body reads the field, so it cannot exist before one does: this
        // is where a runtime entity gets its body, a frame after Play.
        if (m_runtime) {
          m_runtime->refreshBody(entity);
        }
      }

      const auto* range =
        registry.try_get<dunya::objectmodel::SdfPrimitiveRange>(entity);

      const uint32_t primitiveOffset = range == nullptr ? 0u : range->offset;

      const uint32_t primitiveCount = range == nullptr ? 0u : range->count;

      m_recordTable.setRecord(
        recordIndex,
        primitiveOffset,
        primitiveCount,
        registry.get<dunya::objectmodel::Pose>(entity),
        grid,
        registry.get<dunya::objectmodel::BakedVolume>(entity),
        m_frameContext.fieldRepresentation
      );

      if (world.needsBake(entity)) {
        if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
          world.markBaked(entity);
        } else {
          m_recordTable.appendToBakeList(recordIndex);

          if (
            m_runtime && world.needsResample(entity)
            && registry.all_of<
               dunya::objectmodel::RigidBody,
               dunya::field::SampledField>(entity)
          ) {
            const dunya::field::Aabb refit =
              dunya::objectmodel::gridBox(world.primitives(entity));

            world.setSampledField(
              entity,
              dunya::field::bake(
                world.primitives(entity),
                refit.minimum,
                refit.maximum,
                grid.resolution
              )
            );

            // The old shape reads the field that call just replaced.
            m_runtime->refreshBody(entity);
          }
        }
      }

      m_recordEntities.push_back(entity);
      ++recordIndex;
    }

    // One per frame. All of them in one call freezes the window for the whole
    // run - Windows greys it out and the only way to learn whether it is alive
    // is to close it - and a chunk of twenty-five is worse, because 25 x 34 ms
    // is a frame every second and it looks like a hang that redraws. One dent
    // is about 34 ms, so the window stays at roughly 30 fps and the erosion is
    // watchable. The total is the same either way: ten thousand dents at 34 ms
    // is six minutes of real work.
    if (m_pendingDents > 0 && !announcing) {
      constexpr uint32_t DENTS_PER_FRAME = 1u;

      const uint32_t chunk = std::min(m_pendingDents, DENTS_PER_FRAME);

      dent(chunk);
      m_pendingDents -= chunk;

      // A measurement run has nothing to do once the log is written, and
      // leaving it up means the number is read off a window someone has to
      // remember to close.
      if (m_pendingDents == 0 && !m_dentLogPath.empty()) {
        std::cout << "dents complete: " << m_dentsApplied << " logged to "
                  << m_dentLogPath << std::endl;

        glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
      }
    }

    // Before the frame is recorded, because the copy submits and waits: a
    // volume the fragment shader is about to sample has to already hold what
    // the CPU grid says it does.
    {
      const auto uploadStart = std::chrono::steady_clock::now();

      uploadDentedVolumes();

      m_frameUploadMs = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - uploadStart
      )
                          .count();
    }

    m_frameContext.fieldRecordCount = recordIndex;
    m_scene.augmentFrameContext(m_frameContext, world);
    // -----------------------------------

    // Anything that reads the frame back builds no overlay at all, which is
    // what keeps it out of both the reference images and the recordings
    // without the renderer needing to know either exists.
    std::function<void(VkCommandBuffer)> overlayHook;

    if constexpr (enableOverlay) {
      if (!captureHook) {
        m_overlay.begin();
        m_overlay.build();
        m_overlay.end();

        overlayHook = [this](VkCommandBuffer commandBuffer) {
          m_overlay.record(commandBuffer);
        };
      }
    }

    const bool swapChainStale = m_context.window().takeResized()
                                || m_renderer.drawFrame(
                                  m_swapChain,
                                  m_frameContext,
                                  overlayHook,
                                  captureHook
                                );

    if (!swapChainStale) {
      // Bake-list entries are packing slots, so they index the mapping the
      // packing loop recorded, not fields(). A skip consumes no slot.
      for (uint32_t idx : m_recordTable.bakeList()) {
        world.markBaked(m_recordEntities[idx]);
      }
    } else {
      m_swapChain.recreate();
    }
    // After a frame, because the compute bake runs as part of one. Before it,
    // the volumes still hold the CPU bake and the check would be comparing
    // that against itself.
    if (bakeCheckPending) {
      bakeCheckPending = false;
      m_context.device().waitIdle();
      for (dunya::objectmodel::Entity entity : world.fields()) {
        const dunya::objectmodel::SdfGrid& grid =
          registry.get<dunya::objectmodel::SdfGrid>(entity);
        const auto* volume =
          registry.try_get<dunya::objectmodel::BakedVolume>(entity);

        // Nothing was baked for this entity, so there is nothing to check
        // against. Reading the pool at a sentinel would be the bug the
        // component's absence exists to prevent.
        if (volume == nullptr) {
          continue;
        }

        std::span<const dunya::field::Primitive> primitives =
          world.primitives(entity);
        dunya::renderer::VolumeImages images =
          m_volumePool.images(volume->index);
        m_fieldBaker.verifyBake(grid, volume->index, primitives, images);
      }
    }

    if (frameCheck.ran()) {
      glfwSetWindowShouldClose(m_context.window().handle(), GLFW_TRUE);
    }
  }

  m_context.device().waitIdle();

  return frameCheck.failed() ? 1 : 0;
}

void Application::clearCameraInput() noexcept {
  m_cameraInput = {};
}

bool Application::acceptsInput() const noexcept {
  return m_input.enabled() && m_context.window().focused();
}

void Application::handleMouseButtonEvent(
  const dunya::platform::MouseButtonEvent& event
) {
  if (!acceptsInput()) {
    return;
  }

  // The overlay gets first refusal on the cursor, or a click on a slider also
  // carves behind it. Not while looking: the cursor is captured and its
  // reported position is virtual.
  if (!m_looking && m_overlay.wantsMouse()) {
    return;
  }

  if (event.button == GLFW_MOUSE_BUTTON_RIGHT) {
    setLookMode(event.type == dunya::platform::MouseButtonEventType::Pressed);
    return;
  }

  if (
    event.button != GLFW_MOUSE_BUTTON_LEFT
    || event.type != dunya::platform::MouseButtonEventType::Pressed
  ) {
    return;
  }

  // Clicking is for the visible cursor; while looking, the reported position
  // is a virtual one that has nothing to do with the screen.
  if (m_looking) {
    return;
  }

  // While the simulation is running the click is a shot, which is what the
  // demo is: point at the wall and throw something at it. Carving is the
  // authoring gesture and stays on the authoring side of Play, where it
  // cannot be mistaken for the physics doing the damage.
  if (m_runtime && (event.mods & GLFW_MOD_ALT) == 0) {
    fire(aimAtCursor());

    return;
  }

  // Both smooth: a stamp meets the one before it at a crease, and that is what
  // shading shows, whichever direction the material moved.
  m_fieldEditor.edit(
    (event.mods & GLFW_MOD_SHIFT) != 0
      ? dunya::core::FIELD_OP_SMOOTH_UNION
      : dunya::core::FIELD_OP_SMOOTH_SUBTRACTION,
    cursorRay()
  );
}

dunya::objectmodel::World& Application::activeWorld() noexcept {
  return m_runtime ? m_runtime->world() : m_authoredWorld;
}

void Application::releaseAllVolumes() {
  dunya::objectmodel::World& world = activeWorld();

  for (uint32_t slot = 0; slot != m_volumeOwners.size(); ++slot) {
    if (m_volumeOwners[slot] == dunya::objectmodel::INVALID_ENTITY) {
      continue;
    }

    // The component must go with the slot, or the world this belonged to
    // renders through a freed volume the next time it is active.
    if (world.registry().valid(m_volumeOwners[slot])) {
      world.clearBakedVolume(m_volumeOwners[slot]);
    }

    m_volumePool.release(slot);
    m_volumeOwners[slot] = dunya::objectmodel::INVALID_ENTITY;
  }
}

glm::vec3 Application::aimAtPoint(const glm::vec3& target) const {
  const glm::vec3 from = glm::vec3(m_camera.position());

  // At the target rather than along the view: the camera looks down at the
  // scene, so a shot along it would go into the floor. Moving the camera
  // changes which side the ball comes from, never whether it arrives.
  //
  // Gravity is not solved for, so a shot lands a little under what it is aimed
  // at - which is the wall, and lower up a wall topples it better anyway.
  const glm::vec3 muzzle(from.x, m_shotSettings.height, from.z);
  const glm::vec3 toTarget = target - muzzle;

  if (glm::length(toTarget) < 1.0e-3f) {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }

  return glm::normalize(toTarget);
}

glm::vec3 Application::aimAtTarget() const {
  return aimAtPoint(m_shotSettings.aimAt);
}

glm::vec3 Application::aimAtCursor() const {
  const dunya::field::Ray ray = cursorRay();

  // The ray is built from the projection, which is only meaningful once a
  // frame has been assembled. Before that, and while the cursor is captured
  // for looking, the authored target is the honest answer.
  if (glm::length(ray.direction) < 1.0e-3f) {
    return aimAtTarget();
  }

  return glm::normalize(ray.direction);
}

void Application::fire(const glm::vec3& aim) {
  if (!m_runtime) {
    return;
  }

  const Scene::Projectile& shot = m_shotSettings;
  const glm::vec3 from = glm::vec3(m_camera.position());

  const glm::vec3 muzzle(from.x, shot.height, from.z);

  // Oldest out before newest in, so the cap is a cap and not a leak: every
  // ball holds a volume slot and a body, and both are finite.
  if (m_balls.size() == MAX_BALLS) {
    m_runtime->despawn(m_balls.front());
    m_balls.pop_front();
  }

  dunya::objectmodel::World& world = m_runtime->world();

  // At the camera in the ground plane, at the launch height, then a metre
  // down the aim: spawned any nearer and the ball fills the screen on the
  // frame it is fired.
  dunya::objectmodel::Pose pose{};
  pose.position = muzzle + aim * MUZZLE_DISTANCE;

  const dunya::objectmodel::Entity ball = world.createField(pose, shot.grid);

  if (!world.addPrimitive(ball, shot.shape)) {
    static_cast<void>(world.destroyField(ball));
    std::cout << "Primitive arena full, no ball fired\n";

    return;
  }

  // The shared volume, which is what the 95 ms went on: two 128-cubed images
  // created and filled per shot.
  //
  // And baked already, so the pass is skipped rather than repeated. It would
  // write the same sphere back into the slot it came from, and it submits and
  // waits on the queue to do it - ten milliseconds of stall for no change at
  // all. The bounds it also used to fill are uploaded once in start(), for the
  // same reason: neither the geometry nor the slot ever moves.
  if (m_ballVolume != UINT32_MAX) {
    world.setBakedVolume(ball, m_ballVolume);
    world.markBaked(ball);
  }

  m_balls.push_back(ball);

  // The body is made here rather than left to the bake pass: that branch only
  // runs for an entity without a volume, and this one was handed the shared
  // one. Everything it needs already exists, so it can fly this frame.
  //
  // It carries no field of its own. The shape reads the scene's, which is the
  // same geometry, so a copy per ball would be 16 MiB nothing ever reads.
  m_runtime->setBodyShape(ball, m_ballShape);
  m_runtime->setMass(ball, shot.mass);
  m_runtime->launch(ball, aim * shot.speed);
}

void Application::announce(std::string text, Transition transition) {
  m_overlay.notice(std::move(text));
  m_transition = transition;
  m_stall = Stall::Announced;
}

void Application::play() {
  if (m_runtime) {
    return;
  }

  // Identity is preserved across instantiation, so an authored entity id is
  // also valid in the runtime world. The owner table cannot tell them apart,
  // so every slot goes back and the new world bakes its own.
  releaseAllVolumes();

  m_runtime.emplace(m_authoredWorld, m_joltLibrary);

  // An edit has to land in the world that is on screen, and the runtime one
  // is from here on.
  m_fieldEditor.retarget(m_runtime->world());

  std::cout << "Play" << std::endl;
}

void Application::dent(uint32_t count) {
  dunya::objectmodel::World& world = activeWorld();
  const dunya::objectmodel::Entity target = m_scene.deformable();

  if (
    !world.registry().valid(target)
    || !world.registry().all_of<dunya::field::SampledField>(target)
  ) {
    std::cout << "Nothing deformable to dent yet\n";
    return;
  }

  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(world.primitives(target));

  if (!extent.has_value()) {
    std::cout << "The deformable has no bounded extent\n";
    return;
  }

  // On the surface rather than through the volume: a dent below the skin
  // changes nothing visible and would measure the wrong thing. The span is
  // taken across the top face, which is where a shot lands.
  const glm::vec3 span = extent->maximum - extent->minimum;

  dunya::field::SampleBox touched{};

  std::ofstream log;

  if (!m_dentLogPath.empty()) {
    const bool fresh = m_dentsApplied == 0u;

    log.open(m_dentLogPath, fresh ? std::ios::trunc : std::ios::app);

    if (fresh) {
      log << "dent,deform_us,reshape_us,bricks,resident_bytes,primitives,"
             "sweep_settled\n";
    }
  }

  // The R2 sequence: successive multiples of these two fractions fill a square
  // evenly without ever repeating, so the dents spread over the face and land
  // in the same places every run.
  world.patchSampledField(target, [&](dunya::field::SampledField& field) {
    for (uint32_t i = 0; i < count; ++i) {
      const glm::vec2 at = glm::fract(
        static_cast<float>(m_dentsApplied + i)
        * glm::vec2(0.7548777f, 0.5698403f)
      );

      const glm::vec3 centre(
        extent->minimum.x + span.x * at.x,
        extent->maximum.y,
        extent->minimum.z + span.z * at.y
      );

      dunya::field::Primitive cutter = dunya::field::makeSphere(
        centre,
        dunya::core::EDIT_RADIUS,
        1u,
        dunya::core::FIELD_OP_SUBTRACTION
      );

      dunya::field::updateBounds(cutter);

      const auto deformStart = std::chrono::steady_clock::now();

      const dunya::field::DeformReport outcome =
        dunya::field::deformAndRepair(field, cutter);

      const dunya::field::WriteReport& report = outcome.write;

      const auto deformEnd = std::chrono::steady_clock::now();

      touched = dunya::field::merge(touched, report.samples);

      // The collision shape has to follow the craters, or a ball rolls over a
      // hole it should fall into. Incremental: only the bricks the write
      // named.
      auto reshapeEnd = deformEnd;

      if (m_runtime) {
        m_runtime->reshapeAfterDeform(
          target,
          report.brickBegin,
          report.brickEnd
        );

        // Grown by the cutter's own reach, because a body resting on the rim
        // of a crater is beside the samples that moved rather than over them.
        const glm::vec3 pad(dunya::core::EDIT_RADIUS);

        m_runtime->wake(centre - pad * 2.0f, centre + pad * 2.0f);

        reshapeEnd = std::chrono::steady_clock::now();
      }

      if (log.is_open()) {
        const glm::uvec3 moved = report.brickEnd - report.brickBegin;

        // The lattice, plus the per-brick caches the collision shape keeps so
        // a rebuild is cheap. Those are the price of the saving and belong in
        // the same number as everything else this milestone claims is flat.
        const glm::uvec3 counts = dunya::field::brickCounts(field);

        const size_t bricks =
          static_cast<size_t>(counts.x) * counts.y * counts.z;

        const size_t resident =
          field.distances.size() * sizeof(float)
          + field.materials.size() * sizeof(uint32_t)
          + field.brickLipschitz.size() * sizeof(float)
          + field.brickMinimum.size() * sizeof(float)
          + field.brickMaximum.size() * sizeof(float)
          + bricks
              * (sizeof(dunya::physics::SolidIntegral) + sizeof(dunya::physics::FieldSeed));

        log << (m_dentsApplied + i) << ','
            << std::chrono::duration_cast<std::chrono::microseconds>(
                 deformEnd - deformStart
               )
                 .count()
            << ','
            << std::chrono::duration_cast<std::chrono::microseconds>(
                 reshapeEnd - deformEnd
               )
                 .count()
            << ',' << (moved.x * moved.y * moved.z) << ',' << resident << ','
            << world.primitiveCount(target) << ',' << outcome.converged << '\n';
      }
    }
  });

  m_dentsApplied += count;

  const auto found = std::find_if(
    m_pendingUploads.begin(),
    m_pendingUploads.end(),
    [target](const auto& entry) { return entry.first == target; }
  );

  if (found == m_pendingUploads.end()) {
    m_pendingUploads.emplace_back(target, touched);
  } else {
    found->second = dunya::field::merge(found->second, touched);
  }
}

void Application::carve(
  dunya::objectmodel::Entity entity,
  const dunya::field::Primitive& cutter
) {
  dunya::objectmodel::World& world = activeWorld();

  dunya::field::WriteReport report{};

  world.patchSampledField(entity, [&](dunya::field::SampledField& field) {
    report = dunya::field::deformAndRepair(field, cutter).write;
  });

  if (m_runtime) {
    // The collision shape has to follow the crater, or the next ball rolls
    // over a hole it should fall into.
    m_runtime->reshapeAfterDeform(entity, report.brickBegin, report.brickEnd);

    // And whatever was asleep on top of it has to be told, because Jolt only
    // invalidates the contact cache of the body whose shape it just swapped.
    // In world space, so the pose the cutter was expressed against is undone.
    const glm::mat4 model = dunya::objectmodel::model(
      world.registry().get<dunya::objectmodel::Pose>(entity)
    );

    const glm::vec3 centre(cutter.bounds);
    const glm::vec3 reach(cutter.bounds.w * 2.0f);

    const glm::vec3 at = model * glm::vec4(centre, 1.0f);

    m_runtime->wake(at - reach, at + reach);
  }

  const auto found = std::find_if(
    m_pendingUploads.begin(),
    m_pendingUploads.end(),
    [entity](const auto& entry) { return entry.first == entity; }
  );

  if (found == m_pendingUploads.end()) {
    m_pendingUploads.emplace_back(entity, report.samples);
  } else {
    found->second = dunya::field::merge(found->second, report.samples);
  }
}

void Application::applyImpacts() {
  if (!m_runtime) {
    return;
  }

  m_runtime->physics().impacts().drain(m_impacts);

  const entt::registry& registry = m_runtime->world().registry();

  for (const dunya::physics::Impact& impact : m_impacts) {
    const dunya::objectmodel::Entity entity{impact.entity};

    // Both sides of every manifold arrive, and most of them are not
    // deformable: the ball that threw the punch, and the ground before it is
    // tagged. Nothing to do for those.
    if (
      !registry.valid(entity)
      || !registry.all_of<
          dunya::objectmodel::Deformable,
          dunya::field::SampledField>(entity)
    ) {
      continue;
    }

    // Into the field's own frame here rather than at the carve, which is the
    // point of deferring at all: the body keeps moving, and a world-space
    // contact recorded this frame describes nowhere in particular by the time
    // a later frame gets to it. The same crossing FieldEditor::edit makes for
    // a click.
    const glm::mat4 inverseModel = glm::inverse(
      dunya::objectmodel::model(registry.get<dunya::objectmodel::Pose>(entity))
    );

    m_pendingCraters.push_back(
      {entity,
       glm::vec3(inverseModel * glm::vec4(impact.point, 1.0f)),

       // A direction, so the translation is dropped. The pose is rigid, so
       // this stays unit length and the normalize is belt and braces.
       glm::normalize(
         glm::vec3(inverseModel * glm::vec4(impact.outward, 0.0f))
       ),
       impact.impulse}
    );
  }

  if (m_pendingCraters.empty()) {
    return;
  }

  // Hardest first, so a frame that cannot afford all of them spends what it
  // has on the ones that show. Ties break on the entity, which costs nothing
  // and makes the order total: std::sort is not stable, and a demo that has
  // to reproduce should not depend on which of two equal hits it picked.
  std::sort(
    m_pendingCraters.begin(),
    m_pendingCraters.end(),
    [](const PendingCrater& a, const PendingCrater& b) {
      if (a.impulse != b.impulse) {
        return a.impulse > b.impulse;
      }

      return static_cast<uint32_t>(a.entity) < static_cast<uint32_t>(b.entity);
    }
  );

  const size_t budget =
    std::min<size_t>(m_damage.perFrame, m_pendingCraters.size());

  for (size_t i = 0; i != budget; ++i) {
    const PendingCrater& pending = m_pendingCraters[i];
    const dunya::objectmodel::Entity entity = pending.entity;

    // A frame or more may have passed, and the wall has been falling over in
    // the meantime. An entity that has gone takes its craters with it.
    if (
      !registry.valid(entity)
      || !registry.all_of<
          dunya::objectmodel::Deformable,
          dunya::field::SampledField>(entity)
    ) {
      continue;
    }

    // What the object can afford to lose. The primitives rather than the
    // grid, because the grid carries a margin that has nothing to do with how
    // big the object is - and the shortest side is the one that runs out
    // first: a floor is thin and wide, and it is the thickness a crater has to
    // respect. D5 leaves the primitive list describing the object as authored,
    // which is the right thing to measure damage against.
    const std::optional<dunya::field::Aabb> extent =
      dunya::field::boundedExtent(m_runtime->world().primitives(entity));

    const glm::vec3 span =
      extent.has_value() ? extent->maximum - extent->minimum : glm::vec3(1.0f);

    const float widest =
      m_damage.widestFraction * std::min({span.x, span.y, span.z});

    const float radius = std::min(
      m_damage.radiusPerDepth
        * std::clamp(
          m_damage.depthPerImpulse * pending.impulse,
          m_damage.minimumDepth,
          m_damage.maximumDepth
        ),
      widest
    );

    // Recovered from the radius rather than kept, so a crater the object's
    // size capped stays a cap of the right shape instead of a deep puncture
    // in a narrow sphere.
    const float depth = radius / m_damage.radiusPerDepth;

    // Sunk along the inward normal so the sphere's near cap sits at the
    // surface and exactly `depth` of it is inside: the centre goes back by
    // the rest of the radius.
    const glm::vec3 centre = pending.point - pending.outward * (radius - depth);

    dunya::field::Primitive cutter = dunya::field::makeSphere(
      centre,
      radius,
      1u,
      dunya::core::FIELD_OP_SUBTRACTION
    );

    dunya::field::updateBounds(cutter);

    const auto started = std::chrono::steady_clock::now();

    carve(entity, cutter);
    ++m_cratersApplied;

    if (m_demoFrames > 0) {
      const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - started
      )
                            .count();

      std::cout << "  crater on " << static_cast<uint32_t>(entity)
                << "  impulse " << pending.impulse << "  depth " << depth
                << "  radius " << radius << "  " << (micros / 1000.0) << " ms"
                << std::endl;
    }
  }

  // Everything the budget could not reach is dropped rather than carried: a
  // ball that has already punched through has moved on, and a crater that
  // arrives a second later reads as a glitch rather than as damage. The sort
  // above means what goes is always the weakest.
  m_pendingCraters.clear();
}

void Application::reportDemo() const {
  if (m_demoFrameMs.empty()) {
    return;
  }

  std::vector<DemoFrame> sorted = m_demoFrameMs;

  std::sort(
    sorted.begin(),
    sorted.end(),
    [](const DemoFrame& a, const DemoFrame& b) { return a.ms < b.ms; }
  );

  const auto at = [&sorted](double fraction) {
    const size_t index = static_cast<size_t>(fraction * (sorted.size() - 1));
    return sorted[index].ms;
  };

  double total = 0.0;
  size_t overBudget = 0;

  for (const DemoFrame& frame : m_demoFrameMs) {
    total += frame.ms;

    if (frame.ms > 16.6f) {
      ++overBudget;
    }
  }

  std::cout << "\ndemo: " << m_demoFrameMs.size() << " frames, "
            << m_cratersApplied << " craters\n"
            << "  mean " << (total / double(m_demoFrameMs.size()))
            << " ms  median " << at(0.5) << "  p99 " << at(0.99) << "  worst "
            << sorted.back().ms << "\n"
            << "  over 16.6 ms: " << overBudget << " ("
            << (100.0 * double(overBudget) / double(m_demoFrameMs.size()))
            << "%)\n";

  // Named rather than summarised: a spike on a spawn frame and a spike on a
  // crater frame are different bugs, and the mean cannot tell them apart.
  std::cout << "  worst frames:\n";

  const size_t show = std::min<size_t>(6u, sorted.size());

  for (size_t i = 0; i != show; ++i) {
    const DemoFrame& frame = sorted[sorted.size() - 1u - i];

    std::cout << "    frame " << frame.index << "  " << frame.ms
              << " ms   craters " << frame.craters << "  carve "
              << frame.carveMs << "  upload " << frame.uploadMs << "  physics "
              << frame.physicsMs << "  rest "
              << (frame.ms - frame.carveMs - frame.uploadMs - frame.physicsMs)
              << (frame.fired ? "  (spawned a ball)" : "") << "\n";
  }

  std::cout << std::flush;
}

void Application::uploadDentedVolumes() {
  if (m_pendingUploads.empty()) {
    return;
  }

  const entt::registry& registry = activeWorld().registry();

  for (const auto& [entity, box] : m_pendingUploads) {
    if (
      !registry.valid(entity)
      || !registry.all_of<
          dunya::objectmodel::BakedVolume,
          dunya::field::SampledField>(entity)
    ) {
      continue;
    }

    const uint32_t slot =
      registry.get<dunya::objectmodel::BakedVolume>(entity).index;

    const dunya::field::SampledField& field =
      registry.get<dunya::field::SampledField>(entity);

    m_volumePool.upload(m_uploader, slot, field, box);

    // The write that made this box also moved the bricks' gradient bounds,
    // and the march reads those rather than the samples.
    m_recordTable.uploadBounds(m_uploader, slot, field);
  }

  m_pendingUploads.clear();

  // One submission for every copy this frame, and no wait: the frame's own
  // rendering follows on the same queue, and the barriers above order against
  // it. What this replaces was six queue drains per changed object.
  m_uploader.submit();
}

void Application::stop() {
  if (!m_runtime) {
    return;
  }

  releaseAllVolumes();

  m_balls.clear();

  m_runtime.reset();

  m_fieldEditor.retarget(m_authoredWorld);

  std::cout << "Stop" << std::endl;
}

void Application::setLookMode(bool looking) {
  if (m_looking == looking) {
    return;
  }

  m_looking = looking;

  if (!m_looking) {
    clearCameraInput();
  }

  // Entering or leaving capture teleports the cursor, and a delta across that
  // jump would spin the camera (idiom 17).
  m_input.cursorDeltaInvalid();

  glfwSetInputMode(
    m_context.window().handle(),
    GLFW_CURSOR,
    m_looking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
  );
}

// Every panel this process shows, declared where the data it reads lives. They
// capture this, which outlives them.
void Application::registerPanels() {
  // A release build has no panels at all, so there is nothing to register.
  if constexpr (!enableOverlay) {
    return;
  }

  // First, so it is the panel at the top of the stack. What the demo is for:
  // the wall coming apart on the left of the screen, and on the right the two
  // numbers that are the whole claim - the primitive count and the resident
  // bytes, neither of which moves however much of the wall is gone.
  m_overlay.panel("Dunya", [this] {
    ImGui::Text("%.1f fps   %.2f ms", 1000.0 / m_lastFrameMs, m_lastFrameMs);
    ImGui::Separator();

    const dunya::objectmodel::World& world = activeWorld();
    const entt::registry& registry = world.registry();

    size_t resident = 0;
    size_t deformable = 0;

    for (const dunya::objectmodel::Entity entity : world.fields()) {
      if (!registry.all_of<dunya::objectmodel::Deformable>(entity)) {
        continue;
      }

      ++deformable;

      if (
        const auto* field = registry.try_get<dunya::field::SampledField>(entity)
      ) {
        resident += field->distances.size() * sizeof(float)
                    + field->materials.size() * sizeof(uint32_t);
      }
    }

    ImGui::Text("craters carved   %u", m_cratersApplied);
    ImGui::Text("deformable       %zu", deformable);
    ImGui::Separator();

    // The point. A representation that stored edits would have one primitive
    // per crater and a lattice that grew with them; these two lines are what
    // it costs instead, and they do not move.
    ImGui::Text("primitives       %zu", world.pool().size());
    ImGui::Text(
      "lattice          %.1f MiB",
      double(resident) / (1024.0 * 1024.0)
    );

    ImGui::Separator();

    ImGui::TextUnformatted(
      m_runtime ? "click to fire   F5 stops   G resets"
                : "F5 plays   alt+click carves"
    );
    ImGui::TextUnformatted("hold right mouse to look, WASD/QE to fly");
  });

  m_overlay.panel("Damage", [this] {
    if (
      ImGui::SliderFloat(
        "Threshold",
        &m_damage.threshold,
        0.1f,
        20.0f,
        "%.1f m/s"
      )
      && m_runtime
    ) {
      m_runtime->physics().impacts().setThreshold(m_damage.threshold);
    }

    ImGui::SliderFloat(
      "Depth per impulse",
      &m_damage.depthPerImpulse,
      0.0f,
      0.002f,
      "%.5f m"
    );

    ImGui::SliderFloat(
      "Min depth",
      &m_damage.minimumDepth,
      0.0f,
      0.2f,
      "%.3f m"
    );
    ImGui::SliderFloat(
      "Max depth",
      &m_damage.maximumDepth,
      0.0f,
      1.0f,
      "%.3f m"
    );
    ImGui::SliderFloat("Width", &m_damage.radiusPerDepth, 1.0f, 6.0f, "%.2f x");
    ImGui::SliderFloat("Widest", &m_damage.widestFraction, 0.02f, 0.5f, "%.2f");

    int perFrame = int(m_damage.perFrame);
    if (ImGui::SliderInt("Craters per frame", &perFrame, 1, 16)) {
      m_damage.perFrame = uint32_t(perFrame);
    }

    ImGui::Text("Craters  %u", m_cratersApplied);
  });

  m_overlay.panel("Shot", [this] {
    ImGui::SliderFloat("Speed", &m_shotSettings.speed, 1.0f, 80.0f, "%.1f m/s");
    ImGui::SliderFloat("Mass", &m_shotSettings.mass, 1.0f, 3000.0f, "%.0f kg");
    ImGui::SliderFloat3("Target", &m_shotSettings.aimAt.x, -6.0f, 6.0f);
    ImGui::Text("Balls  %zu / %zu", m_balls.size(), MAX_BALLS);

    if (ImGui::Button("Fire")) {
      fire(aimAtTarget());
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset wall") && m_stall == Stall::None && m_runtime) {
      announce("Resetting", Transition::Restart);
    }
  });

  m_overlay.panel("Frame", [this] {
    ImGui::Text(
      "%.2f ms  %.0f fps",
      m_lastFrameMs,
      m_lastFrameMs > 0.0 ? 1000.0 / m_lastFrameMs : 0.0
    );
    ImGui::Text(
      "%ux%u",
      m_swapChain.extent().width,
      m_swapChain.extent().height
    );

    size_t primitiveCount = m_authoredWorld.pool().size();

    ImGui::Text("%zu primitives", primitiveCount);
    ImGui::Text(
      "%s",
      m_frameContext.fieldRepresentation == dunya::core::FIELD_ANALYTIC
        ? "analytic"
        : "sampled"
    );
  });

  m_overlay.panel("March", [this] {
    dunya::renderer::MarchParams& march = m_frameContext.march;

    // Logarithmic where the useful range spans orders of magnitude: a linear
    // slider from 0.0001 to 0.01 spends nearly all of its travel in values
    // that make the march crawl.
    ImGui::SliderFloat(
      "epsilon",
      &march.epsilon,
      0.0001f,
      0.01f,
      "%.5f",
      ImGuiSliderFlags_Logarithmic
    );
    ImGui::SliderFloat(
      "gradient",
      &march.gradientEpsilon,
      0.001f,
      0.1f,
      "%.4f"
    );

    // Below 1 is plain sphere tracing and above 2 is unstable even when the
    // estimator is conservative.
    ImGui::SliderFloat("omega", &march.omega, 1.0f, 2.0f);

    // Above 1 would trust a value trilinear interpolation is known to
    // overestimate, which is how a march steps through a surface.

    ImGui::SliderFloat("max distance", &march.maxDistance, 10.0f, 500.0f);
    ImGui::SliderFloat(
      "shadow distance",
      &march.shadowMaxDistance,
      1.0f,
      100.0f
    );
    ImGui::SliderFloat("shadow sharpness", &march.shadowSharpness, 1.0f, 64.0f);

    int iterations = static_cast<int>(march.maxIterations);
    if (ImGui::SliderInt("max iterations", &iterations, 32, 2000)) {
      march.maxIterations = static_cast<uint32_t>(iterations);
    }
  });
}

dunya::field::Ray Application::cursorRay() const {
  const dunya::platform::Cursor cursor = m_input.cursor();
  const VkExtent2D extent = m_swapChain.extent();

  // Vulkan's NDC y runs downward because the projection flips it, and GLFW
  // reports the cursor from the top left, so both axes map without a flip.
  const glm::vec2 ndc(
    2.0f * static_cast<float>(cursor.x) / static_cast<float>(extent.width)
      - 1.0f,
    2.0f * static_cast<float>(cursor.y) / static_cast<float>(extent.height)
      - 1.0f
  );

  const glm::mat4 viewProj = m_frameContext.proj * m_frameContext.view;

  return dunya::field::screenPointToRay(
    glm::inverse(viewProj),
    glm::vec3(m_frameContext.cameraPos),
    ndc
  );
}

void Application::handleKeyEvent(const dunya::platform::KeyEvent& event) {
  // Typing into a text field must not also fly the camera. Escape is exempt
  // because it is how the cursor is handed back, and a UI that could swallow it
  // would be a UI you cannot leave.
  if (m_overlay.wantsKeyboard() && event.key != GLFW_KEY_ESCAPE) {
    return;
  }

  if (
    event.key == GLFW_KEY_ESCAPE
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    m_input.toggleEnabled();

    clearCameraInput();

    // The cursor now belongs to look mode, so escape leaves it rather than
    // setting the mode itself.
    setLookMode(false);
  }

  if (
    event.key == GLFW_KEY_P
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_frameContext.mode = nextPipelineType(m_frameContext.mode);
    std::cout << "Pipeline mode switched to: " << (int)m_frameContext.mode
              << '\n';
  }

  if (
    event.key == GLFW_KEY_B
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_fieldEditor.stress(10);
    return;
  }

  if (
    event.key == GLFW_KEY_N
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    dent(m_input.isDown(GLFW_KEY_LEFT_SHIFT) ? 100u : 1u);
    return;
  }

  if (
    event.key == GLFW_KEY_F5
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    // Announced rather than done here: both directions stall the loop, and a
    // frame has to be presented before the stall for the message to be seen.
    if (m_stall == Stall::None) {
      announce(
        m_runtime ? "Leaving play mode" : "Entering play mode",
        m_runtime ? Transition::ToAuthoring : Transition::ToRuntime
      );
    }

    return;
  }

  if (
    event.key == GLFW_KEY_G
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    if (m_stall == Stall::None && m_runtime) {
      announce("Resetting", Transition::Restart);
    }

    return;
  }

  if (
    event.key == GLFW_KEY_F
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    fire(aimAtCursor());

    return;
  }

  if (
    event.key == GLFW_KEY_R
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_reloadRequested = true;
    return;
  }

  if (
    event.key == GLFW_KEY_V
    && event.type == dunya::platform::KeyEventType::SinglePressed
  ) {
    m_context.device().waitIdle();
    m_swapChain.setUncapped(!m_swapChain.uncapped());
    std::cout << "Present mode: "
              << (m_swapChain.uncapped() ? "immediate" : "fifo/mailbox")
              << '\n';
    return;
  }

  if (
    event.key == GLFW_KEY_Z
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
      m_fieldEditor.undo();
    }
  }

  if (
    event.key == GLFW_KEY_Y
    && event.type == dunya::platform::KeyEventType::Pressed
  ) {
    if (m_input.isDown(GLFW_KEY_LEFT_CONTROL)) {
      m_fieldEditor.redo();
    }
  }

  bool* state = nullptr;

  switch (event.key) {
    case GLFW_KEY_W:
      state = &m_cameraInput.forward;
      break;

    case GLFW_KEY_S:
      state = &m_cameraInput.back;
      break;

    case GLFW_KEY_A:
      state = &m_cameraInput.left;
      break;

    case GLFW_KEY_D:
      state = &m_cameraInput.right;
      break;

    case GLFW_KEY_E:
      state = &m_cameraInput.up;
      break;

    case GLFW_KEY_Q:
      state = &m_cameraInput.down;
      break;
    default:
      return;
  }

  switch (event.type) {
    case dunya::platform::KeyEventType::Pressed:
      // Movement belongs to look mode, the same way it does in a scene view.
      *state = acceptsInput() && m_looking;
      break;

    case dunya::platform::KeyEventType::Released:
      *state = false;
      break;

    default:
      break;
  }
}
