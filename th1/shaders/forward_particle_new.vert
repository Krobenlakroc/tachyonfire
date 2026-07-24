#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;

out vec2 ID;
out vec2 outTexcoord;
out vec3 outPosition;
out vec3 outNormal;
out vec3 outTangent;
out float colorscale;
out float alphascale;
out float blend;

uniform mat4 modelViewprojection;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 viewMatrixInv;

uniform int dynamic_cutoff;
uniform vec3 eyePos;

vec3 goffset =vec3(-23,-75,196);
vec3 gridsize = vec3(43,23,53);
float griddist = 12;

vec3 quantify(vec3 position)
{
  return floor((position - goffset)/griddist  + 0.5*gridsize) - vec3(1,1,1);
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

void main()
{


  //mat4 modelView = gxl3d_ViewMatrix*gxl3d_ModelMatrix;


  // mat4 modelView = view*inTransform;
  //
  // // First colunm.
  // modelView[0][0] = inTransform[0][0];
  // modelView[0][1] = inTransform[0][1];
  // modelView[0][2] = inTransform[0][2];
  //
  //
  //
  //   modelView[1][0] = inTransform[1][0];
  //   modelView[1][1] = inTransform[1][1];
  //   modelView[1][2] = inTransform[1][2];
  //
  //
  // // Thrid colunm.
  // modelView[2][0] = inTransform[2][0];
  // modelView[2][1] = inTransform[2][1];
  // modelView[2][2] = inTransform[2][2];
  float overbright = inTransform[2].y;
  alphascale = inTransform[2].w;

  colorscale = overbright;
  float motion = inTransform[2].x;
  float p_texture = inTransform[2].z;
  vec3 p_velocity = inTransform[1].xyz;
  vec3 p_position = inTransform[0].xyz;
  float rotation = inTransform[0].w;
  vec2 scale = vec2(inTransform[1].w);
  float p_texturebase = inTransform[3].x;
  blend = inTransform[3].y;
  float constrain = inTransform[3].z;

  mat2 rot = mat2(
		cos(rotation), -sin(rotation),
		sin(rotation), cos(rotation)
		);
    vec3 quadPos = inPosition;
     quadPos.xy = rot*quadPos.xy;
    quadPos.xy *= scale;
    quadPos.xy = quadPos.xy*(1.0 - constrain) + (quadPos.xy*vec2(1.0,motion))*constrain;

    //vec3 velocity = p_velocity;
    vec3 velocity = mat3(view)*p_velocity;
    quadPos = (quadPos + dot(quadPos, velocity) * velocity * motion)*(1.0 - constrain) + quadPos*constrain;
	// quadPos += dot(quadPos, velocity) * velocity * motion;

//  vec4 p = modelView*vec4(inPosition.xyz*scale ,1.0);

  // gl_Position = (proj*vec4(p.xyz,1.0));
  outTexcoord = vec2(inTexcoord.x,1 -inTexcoord.y);
  ID = vec2(inTransform[3].x,p_texture);

  // mat4 modelTransform = viewMatrixInv*modelView;
  // outPosition = ((modelTransform)*vec4(inPosition.xyz ,1.0)).xyz;
  //
  // outTangent = normalize(vec3(modelTransform * vec4(inTangent,   0.0)));
  // // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
  // outNormal = normalize(vec3(modelTransform * vec4(inNormal,    0.0)));
  //
  //  T = normalize(vec3(inTransform * vec4(inTangent,   0.0)));
  //  // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
  //  N = normalize(vec3(inTransform * vec4(inNormal,    0.0)));



  //  // copy to output:
	// vec4 trpos = vec4(p_position,1.0);
  // //trpos.xyz += quadPos.xyz;
  // // mat4 viewdup = view;
  //
	// trpos = view*trpos;
  // trpos.xyz += quadPos.xyz*vec3(1,1,0);
	// outPosition = (viewMatrixInv*vec4(trpos.xyz, 1)).xyz;
	// gl_Position = proj*trpos;

    vec3 particleDirection = normalize(p_velocity);
    vec3 cameraDir = normalize(eyePos - p_position);
    mat3 rotMatrix;

    vec3 xaxis = cross(cameraDir, particleDirection);
    xaxis = normalize(xaxis);

    vec3 zaxis = cross(xaxis, particleDirection);
    zaxis = normalize(zaxis);

    rotMatrix = mat3(xaxis,particleDirection,zaxis);


    vec3 quadPos_d = rotMatrix * quadPos;

    vec3 newPos =  quadPos_d + p_position;


  //   gl_Position = modelViewprojection*vec4(newPos,1.0);
  //
  // outPosition = newPos.xyz;

  vec4 trpos = vec4(p_position,1.0);
  //trpos.xyz += quadPos.xyz;
  // mat4 viewdup = view;

  trpos = view*trpos;
  trpos.xyz += quadPos.xyz;
  outPosition = (viewMatrixInv*vec4(trpos.xyz, 1)).xyz*(1.0 - constrain) + newPos.xyz*constrain ;
  gl_Position = proj*trpos*(1.0 - constrain) + modelViewprojection*vec4(newPos,1.0)*constrain;


  outTangent = normalize(vec3(viewMatrixInv * vec4(inTangent,   0.0)));
  // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
  outNormal = normalize(vec3(viewMatrixInv * vec4(inNormal,    0.0)));
   //
   // T = normalize(vec3(viewMatrixInv * vec4(inTangent,   0.0)));
   // // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
   // N = normalize(vec3(viewMatrixInv * vec4(inNormal,    0.0)));



}
