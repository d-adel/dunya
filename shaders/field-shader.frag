#version 450

const int MAX_PRIMITIVES = 128;
const float eps = 0.001;
const int maxIter = 800;
const float normalSampleOffset = 0.01;
const float bias = 0.01;

struct Primitive {
  mat4 inverseModel;
  vec4 shape;
  uvec4 shapeConfig;
};

layout(std140, set = 0, binding = 0) uniform
FieldScene { Primitive primitives[MAX_PRIMITIVES]; } scene;

layout(std140, set = 0, binding = 1) uniform FieldFrame {
  mat4 inverseViewProj;
  mat4 viewProj;
  vec4 cameraPos;
  uvec4 primitiveCount;
} frame;

layout(location = 0) in vec4 ndc;
layout(location = 0) out vec4 outColor;

float smin(float a, float b, float k)
{
    float h = clamp(
        0.5 + 0.5 * (b - a) / k,
        0.0,
        1.0
    );

    return mix(b, a, h) - k * h * (1.0 - h);
}

float sdBox(vec3 p, vec3 center, vec3 halfSize)
{
    vec3 q = abs(p - center) - halfSize;

    float outsideDistance = length(max(q, vec3(0.0)));
    float insideDistance = min(max(q.x, max(q.y, q.z)), 0.0);

    return outsideDistance + insideDistance;
}

vec2 minMat(vec2 a, vec2 b) {
  return a.x < b.x ? a : b;
}

float primitiveDistance(vec3 p, Primitive prim) {
  vec3 local = ((prim.inverseModel) * vec4(p, 1.0)).xyz;
  switch(prim.shapeConfig.x) {
    case 0u:
      return length(local) - prim.shape.x;
    case 1u:
      return sdBox(local, vec3(0), prim.shape.xyz);
    case 2u:
      return local.y;
    default:
      return 1e9;
  }
}

vec2 sceneDistance(vec3 p) {
  vec2 acc = vec2(1e9, 0);
  for (uint i = 0u; i < frame.primitiveCount.x; ++i) {
    vec2 cur = vec2(primitiveDistance(p, scene.primitives[i]),
    float(scene.primitives[i].shapeConfig.y));
    switch(scene.primitives[i].shapeConfig.z) {
      case 1u:
        {
          float k = scene.primitives[i].shape.w;
          acc = vec2(smin(acc.x, cur.x, k), acc.x < cur.x ? acc.y : cur.y);
        }
        break;
      case 2u:
        acc = acc.x > cur.x ? acc : cur;
        break;
      case 3u:
        acc = vec2(max(acc.x, -cur.x), acc.y);
        break;
      default:
        acc = minMat(acc, cur);
        break;
    }
  }

  return acc;
}

vec3 estimateNormal(vec3 p)
{
    vec3 offsetX = vec3(normalSampleOffset, 0.0, 0.0);
    vec3 offsetY = vec3(0.0, normalSampleOffset, 0.0);
    vec3 offsetZ = vec3(0.0, 0.0, normalSampleOffset);

    return normalize(vec3(
        sceneDistance(p + offsetX).x - sceneDistance(p - offsetX).x,
        sceneDistance(p + offsetY).x - sceneDistance(p - offsetY).x,
        sceneDistance(p + offsetZ).x - sceneDistance(p - offsetZ).x
    ));
}

bool march(vec3 origin, vec3 direction, out vec3 hitPosition, out float materialId)
{
    float distanceTravelled = 0;

    for (int i = 0; i < maxIter; ++i)
    {
        vec3 p =
            origin +
            direction * distanceTravelled;

        vec2 distanceToSurface = sceneDistance(p);

        if (distanceToSurface.x <= eps)
        {
            hitPosition = p;
            materialId = distanceToSurface.y;
            return true;
        }

        distanceTravelled += distanceToSurface.x;

        if (distanceTravelled > 100.0)
        {
            break;
        }
    }

    return false;
}

vec3 albedo(float materialId) {
  if (int(materialId + 0.5) == 0) {
    return vec3(0.5, 0.0, 0.3);
  } else {
    return vec3(0.7, 0.6, 0.3);
  }
}

void main()
{

    vec2 inside = sceneDistance(frame.cameraPos.xyz);
    if (inside.x <= frame.cameraPos.w) {
      gl_FragDepth = 0.0;
      outColor = vec4(albedo(inside.y), 1);
      return;
    }

    vec4 clipPosition = vec4(ndc.xy, 1.0, 1.0);

    vec4 worldPosition =
        frame.inverseViewProj * clipPosition;

    worldPosition /= worldPosition.w;

    vec3 direction = normalize(
        worldPosition.xyz - frame.cameraPos.xyz
    );

    vec3 hitPosition;
    float materialId;
    if (march(frame.cameraPos.xyz, direction, hitPosition, materialId))
    {
      vec3 surfaceAlbedo = albedo(materialId);

      vec4 clip = frame.viewProj * vec4(hitPosition, 1.0);
      float depth = clip.z / clip.w;
      if (depth <= 0 || isinf(depth) || isnan(depth)) {
        discard;
      }
      gl_FragDepth = depth;

      vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));
      vec3 normal = estimateNormal(hitPosition);
      float diffuse = max(0.0, dot(normal, lightDir));


      vec3 shadowHit;
      bool shadowed = false;
      if (diffuse > 0) {
        if (march(hitPosition + normal * bias, lightDir, shadowHit, materialId)) {
          shadowed = true;
        }
      }

      float ambient = 0.005;

      vec3 color = vec3(surfaceAlbedo * (ambient + diffuse * (shadowed ? 0.0 : 1.0)));
      outColor = vec4(color, 1.0);
    }
    else
    {
        discard;
    }
}
