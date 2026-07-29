#version 450

layout(location = 0) out vec4 ndc;

void main() {
  int idx = gl_VertexIndex;
  if (idx == 0) ndc = vec4(-1,-1,0,1);
  else if (idx == 1) ndc = vec4(3,-1,0,1);
  else ndc = vec4(-1,3,0,1);

  gl_Position = ndc;
}
