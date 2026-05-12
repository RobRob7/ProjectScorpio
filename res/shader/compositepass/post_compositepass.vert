#version 460 core

#ifdef VULKAN
  #define VERT_ID gl_VertexIndex
#else
  #define VERT_ID gl_VertexID
#endif

layout(location = 0) out vec2 vUV;

void main()
{
    vec2 pos;
    if (VERT_ID == 0) pos = vec2(-1.0, -1.0);
    if (VERT_ID == 1) pos = vec2( 3.0, -1.0);
    if (VERT_ID == 2) pos = vec2(-1.0,  3.0);

    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
