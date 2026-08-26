/* The single definition of stb's implementations, for the whole project.
 *
 * texture/stb_image_impl.cc used to hold a second copy. Once the renderer links
 * this library the two collide - every stb symbol defined twice - so the other
 * one was deleted and texture loading now resolves through here. This is the
 * copy that survived because it is the one the tests can reach without a device.
 *
 * The warning state is pushed and popped around the includes rather than turned
 * off for the whole file in CMake: a per-source /W0 collides with the target's
 * /W4 and MSVC reports the collision itself (D9025), which trades one warning
 * for another. Third-party code opts out here; ours stays at /W4 /WX.
 */

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
