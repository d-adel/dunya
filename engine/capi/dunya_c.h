#pragma once

#include <stdint.h>

#if defined(_WIN32)
  #define DUNYA_C_API __declspec(dllexport)
#else
  #define DUNYA_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DunyaSession DunyaSession;

DUNYA_C_API DunyaSession* dunya_session_create(
  void* windowHandle,
  const char* projectRoot,
  const char* world
);

DUNYA_C_API void dunya_session_destroy(DunyaSession* session);

DUNYA_C_API int32_t dunya_session_resize(DunyaSession* session);

DUNYA_C_API int32_t
dunya_session_retarget(DunyaSession* session, void* windowHandle);

DUNYA_C_API int32_t dunya_session_render(DunyaSession* session);

DUNYA_C_API int32_t dunya_session_extent(
  const DunyaSession* session,
  uint32_t* width,
  uint32_t* height
);

DUNYA_C_API int32_t dunya_session_entities(
  const DunyaSession* session,
  uint32_t* entities,
  uint32_t capacity,
  uint32_t* count
);

DUNYA_C_API int32_t dunya_session_entity_components(
  const DunyaSession* session,
  uint32_t entity,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
);

DUNYA_C_API int32_t dunya_session_component(
  const DunyaSession* session,
  uint32_t entity,
  const char* component,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
);
DUNYA_C_API const void* dunya_api(void);

typedef void (*DunyaLogSink)(const char* message);

DUNYA_C_API void dunya_set_log_sink(DunyaLogSink sink);

DUNYA_C_API void* dunya_session_schedule(DunyaSession* session);

DUNYA_C_API void* dunya_session_world(DunyaSession* session);

DUNYA_C_API int32_t dunya_project_create(const char* root, const char* name);

DUNYA_C_API void dunya_session_camera_orbit(
  DunyaSession* session,
  float yaw,
  float pitch
);

DUNYA_C_API void dunya_session_camera_pan(
  DunyaSession* session,
  float x,
  float y
);

DUNYA_C_API void dunya_session_camera_zoom(DunyaSession* session, float delta);

DUNYA_C_API void dunya_session_camera_focus(
  DunyaSession* session,
  uint32_t entity
);

DUNYA_C_API void dunya_session_align_to_scene_camera(DunyaSession* session);

DUNYA_C_API uint32_t
dunya_session_pick(DunyaSession* session, float x, float y);

DUNYA_C_API uint32_t dunya_session_create_light(DunyaSession* session);

DUNYA_C_API uint32_t dunya_session_create_environment(DunyaSession* session);

DUNYA_C_API uint32_t dunya_session_create_camera(
  DunyaSession* session,
  const float* position,
  const float* target,
  float verticalFov
);

DUNYA_C_API uint32_t dunya_session_create_sdf(
  DunyaSession* session,
  const float* position,
  const float* rotation,
  const uint32_t* resolution,
  float margin
);

DUNYA_C_API int32_t dunya_session_add_primitive(
  DunyaSession* session,
  uint32_t entity,
  uint64_t material,
  const void* edit
);

DUNYA_C_API int32_t
dunya_session_set_static(DunyaSession* session, uint32_t entity);

DUNYA_C_API int32_t
dunya_session_set_deformable(DunyaSession* session, uint32_t entity);

DUNYA_C_API int32_t
dunya_session_destroy_entity(DunyaSession* session, uint32_t entity);

DUNYA_C_API int32_t dunya_session_save(DunyaSession* session);

DUNYA_C_API int32_t dunya_session_play(DunyaSession* session);

DUNYA_C_API int32_t dunya_session_stop(DunyaSession* session);

DUNYA_C_API int32_t dunya_session_playing(const DunyaSession* session);

DUNYA_C_API uint32_t dunya_session_material_count(const DunyaSession* session);

DUNYA_C_API uint64_t
dunya_session_material_at(const DunyaSession* session, uint32_t index);

DUNYA_C_API uint64_t dunya_session_add_material(
  DunyaSession* session,
  const float* baseColor,
  float metallic,
  float roughness
);

DUNYA_C_API void dunya_session_set_supersample(
  DunyaSession* session,
  float scale
);

DUNYA_C_API void dunya_session_show_grid(
  DunyaSession* session,
  int32_t visible
);

DUNYA_C_API int32_t
dunya_session_open_world(DunyaSession* session, const char* name);

DUNYA_C_API int32_t
dunya_session_new_world(DunyaSession* session, const char* name);

DUNYA_C_API int32_t
dunya_session_save_as(DunyaSession* session, const char* name);

DUNYA_C_API int32_t dunya_session_package(
  DunyaSession* session,
  const char* runtimeExecutable,
  const char* output,
  const char* worlds,
  char* executable,
  uint32_t capacity,
  uint32_t* length
);

DUNYA_C_API int32_t dunya_session_worlds(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
);

DUNYA_C_API int32_t dunya_session_assets(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
);

DUNYA_C_API int32_t dunya_session_current_world(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
);

DUNYA_C_API uint64_t dunya_session_import_asset(
  DunyaSession* session,
  const char* file,
  const char* type
);

DUNYA_C_API const char* dunya_last_error(void);

#ifdef __cplusplus
}
#endif
