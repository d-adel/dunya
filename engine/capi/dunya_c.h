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
DUNYA_C_API const char* dunya_last_error(void);

#ifdef __cplusplus
}
#endif
