#version 450

layout(push_constant) uniform PushConstantsBlock
{
    mat4 inverseViewProj;
    vec4 cameraPos;
} pushConstants;

layout(location = 0) in vec4 ndc;
layout(location = 0) out vec4 outColor;

const vec3 sphereCenter = vec3(0.0, 0.0, 0.0);
const vec3 boxCenter = vec3(1.2, 0.0, 0.0);
const vec3 planeCenter = vec3(0.0, -2.0, 0.0);
const float radius = 1.0;
const float eps = 0.001;
const int maxIter = 800;
const float normalSampleOffset = 0.01;
const float bias = 0.01;

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

float sdPlane(vec3 p, vec3 p0, vec3 n) {
    return dot(n, p - p0);
}

float minDistSphere(vec3 p) {
  return distance(p, sphereCenter) - radius;
}

float minDistBox(vec3 p)
{
    return sdBox(
        p,
        boxCenter,
        vec3(0.5, 0.5, 0.5)
    );
}

float minDistPlane(vec3 p) {
  return sdPlane(
    p,
    planeCenter,
    vec3(0, 1.0, 0.0)
  );
}

float shapeUnion(vec3 p) {
  return min(minDistSphere(p), minDistBox(p));
}

float smoothUnion(vec3 p) {
  return smin(minDistSphere(p), minDistBox(p), 0.4);
}

float shapeIntersection(vec3 p) {
  return max(minDistSphere(p), minDistBox(p));
}

float shapeSubstraction(vec3 p) {
  return max(minDistSphere(p), -minDistBox(p));
}

vec2 minMat(vec2 a, vec2 b) {
  return a.x < b.x ? a : b;
}

vec2 sceneDistance(vec3 p) {
  vec2 a = vec2(smoothUnion(p), 0);
  vec2 b = vec2(minDistPlane(p), 1);
  return minMat(a, b);
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
    float distanceTravelled = 0.0;

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

void main()
{
    vec4 clipPosition = vec4(ndc.xy, 1.0, 1.0);

    vec4 worldPosition =
        pushConstants.inverseViewProj * clipPosition;

    worldPosition /= worldPosition.w;

    vec3 direction = normalize(
        worldPosition.xyz - pushConstants.cameraPos.xyz
    );

    vec3 hitPosition;
    float materialId;
    if (march(pushConstants.cameraPos.xyz, direction, hitPosition, materialId))
    {
      vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));
      vec3 normal = estimateNormal(hitPosition);
      float diffuse = max(0.0, dot(normal, lightDir));

      vec3 albedo;
      if (int(materialId + 0.5) == 0) {
        albedo = vec3(0.5, 0.0, 0.3);
      } else {
        albedo = vec3(0.7, 0.6, 0.3);
      }

      vec3 shadowHit;
      bool shadowed = false;
      if (diffuse > 0) {
        if (march(hitPosition + normal * bias, lightDir, shadowHit, materialId)) {
          shadowed = true;
        }
      }

      float ambient = 0.005;

      vec3 color = vec3(albedo * (ambient + diffuse * (shadowed ? 0.0 : 1.0)));
      outColor = vec4(color, 1.0);
    }
    else
    {
        outColor = vec4(0.01, 0.01, 0.01, 1.0);
    }
}
