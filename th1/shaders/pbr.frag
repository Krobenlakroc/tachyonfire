#version 450 core
layout(early_fragment_tests) in;
in vec2 outTexcoord;
in vec3 outPosition;
in vec4 lightSpacePosition;
in mat3 TBN;
in vec2 ID;
in vec3 surfNorm;

layout (location = 0) out vec4 fragColor;
const float constantbias = 0.00002;
layout(binding=6) uniform sampler2DArray materials[3];
// uniform sampler2D albedoMap;
// uniform sampler2D normalMap;
// uniform sampler2D metallicMap;
// uniform sampler2D roughnessMap;
// uniform sampler2D aoMap;
uniform vec3 eyePos;
const float PI = 3.14159265359;
layout(binding=0) uniform sampler2D irradianceMap;
layout(binding=3) uniform sampler2D prefilterMap;
layout(binding=4) uniform sampler2D brdfLUT;
uniform vec2 invWindow;
layout(binding=24) uniform sampler2D shadowMap;
uniform vec3 shadowPos;
float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

vec2 VogelDiskSample(float sampleIndex, float samplesCount, float phi)
{
  float GoldenAngle = 2.4f;

  float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount);
  float theta = sampleIndex * GoldenAngle + phi;

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec2(r * cosine, r * sine);
}

float InterleavedGradientNoise(vec2 position_screen)
{
  vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
  return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

// float AvgBlockersDepthToPenumbra(float lightSize, float z_shadowMapView, float avgBlockersDepth)
// {
//   return lightSize * (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
// }

float AvgBlockersDepthToPenumbra(float z_shadowMapView, float avgBlockersDepth)
{
  float penumbra = (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
  penumbra *= penumbra;
  return clamp( penumbra*175000.0,0.05,1.0);
}

float shadowmaxsize = 0.15;
float penumbrasize = 0.05;
float Penumbra(float gradientNoise, vec2 shadowMapUV, float z_shadowMapView, float samplesCount,vec2 deltas)
{
  float avgBlockersDepth = 0.0f;
  float blockersCount = 0.0f;

  for(float i = 0; i < samplesCount; i++)
  {
    vec2 sampleUV = VogelDiskSample(i, samplesCount, gradientNoise);
    vec2 offset =penumbrasize * sampleUV;
    sampleUV = shadowMapUV + offset;
    float bias = (1024)*deltas.x*offset.x + (1024)*deltas.y*offset.y;
    float sampleDepth = texture(shadowMap, sampleUV).r;

    // if(sampleDepth < z_shadowMapView)
    // {
    //   avgBlockersDepth += sampleDepth;
    //   blockersCount += 1.0f;
    // }
    float conditional = when_gt(z_shadowMapView,sampleDepth + bias -constantbias);
    avgBlockersDepth += conditional*sampleDepth;
    blockersCount += conditional;
  }

  return when_gt(blockersCount,0.0)*AvgBlockersDepthToPenumbra(z_shadowMapView, avgBlockersDepth/max(blockersCount,0.001));

}

float ShadowCalculation(vec4 fragPosLightSpace,float NdotL)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
  //  vec4 closestDepths = textureGather(shadowMap, projCoords.xy,0);
//  float bias = 0.00001;//clamp(0.001*tan(acos(NdotL)),0,0.1);//max(0.05 * (1.0 - NdotL), 0.005);

//float bias = tan(acos(NdotL));
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
  //  vec2 q = vec2(dFdx(currentDepth),dFdy(currentDepth)*1024);
    // float bias = max(q.x,q.y)*0.0001 + 0.00001;
    vec3 DDX = vec3(dFdx(projCoords.x),dFdx(projCoords.y),dFdx(projCoords.z));
    vec3 DDY = vec3(dFdy(projCoords.y),dFdy(projCoords.y),dFdy(projCoords.z));

    vec2 righttexel = vec2(1/1024,0);
    vec2 uptexel = vec2(0,1/1024);

    mat2 toscreen = inverse(mat2(DDX.x,DDY.x,DDX.y,DDY.y));
    vec2 rightratio = toscreen*righttexel;
    vec2 upratio = toscreen*uptexel;
    float rightdelta = rightratio.x * DDX.z
            + rightratio.y * DDY.z;
    float updelta = upratio.x*DDX.z + upratio.y*DDY.z;
    vec2 deltas = vec2(rightdelta,updelta);


    // check whether current frag pos is in shadow
    float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;
    float penumbra = Penumbra(gradientNoise, projCoords.xy, currentDepth , 16,deltas);

    float shadow = 0.0f;

for(float i = 0; i < 16; i+=1)
{
  vec2 sampleUV = VogelDiskSample(i, 8, gradientNoise);
  vec2 offset = sampleUV * penumbra * shadowmaxsize;
  sampleUV = projCoords.xy + offset;

  float bias = (1024)*deltas.x*offset.x + (1024)*deltas.y*offset.y;
  float closestDepths = texture(shadowMap, sampleUV,0).r;
  shadow += when_gt(currentDepth + bias - constantbias , closestDepths) ;

  // shadow +=  when_gt(currentDepth - bias , closestDepths.x) + when_gt(currentDepth - bias , closestDepths.y) +
  //  when_gt(currentDepth - bias , closestDepths.z) + when_gt(currentDepth - bias , closestDepths.w);
}
shadow /= (16.0f);
return shadow*(1-when_gt(projCoords.z,1.0));
    // float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // float shadow = when_gt(currentDepth - bias , closestDepths.x)*0.75 + when_gt(currentDepth - bias , closestDepths.y)*0.75 +
    // when_gt(currentDepth - bias , closestDepths.z)*0.75 + when_gt(currentDepth - bias , closestDepths.w)*0.75;
    //
    // return shadow/4;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0,float roughness)
{
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
    //return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
  vec4 a = texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y)));
  vec4 b = texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y + 1)));
  vec3 normal = b.rgb;//texture(normalMap, outTexcoord).rgb;
  normal = normal * 2.0 - 1.0;
  normal = normalize(TBN * normal);
  float metallic  = a.a;//texture(metallicMap, outTexcoord).r;
  float roughness = b.a;//texture(roughnessMap, outTexcoord).r;
  float ao        =1;//= texture(aoMap, outTexcoord).r;
  vec3 albedo     = pow(a.rgb,vec3(2.2));//pow(texture(albedoMap, outTexcoord).rgb, vec3(2.2));


  vec3 N = normalize(normal);
    vec3 V = normalize(eyePos - outPosition);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    vec3 lightpos = vec3(3.861059*2 ,-65.025368*2, -60.574471*2);
    vec3 lightcolor = vec3(30000/6,30000/6,30000/6);

    //loop over 6 lights
    // for (int i = 0 ; i < 6;i++)
    // {
    //     // calculate per-light radiance
    //     vec3 L = normalize(lightpos - outPosition);
    //     vec3 H = normalize(V + L);
    //     float d    = length(lightpos - outPosition);
    //     float attenuation = 1.0 / (d * d);
    //     vec3 radiance     = lightcolor * attenuation;
    //
    //     // cook-torrance brdf
    //     float NDF = DistributionGGX(N, H, roughness);
    //     float G   = GeometrySmith(N, V, L, roughness);
    //     vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0,roughness);
    //
    //     vec3 kS = F;
    //     vec3 kD = vec3(1.0) - kS;
    //     kD *= 1.0 - metallic;
    //
    //     vec3 numerator    = NDF * G * F;
    //     float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    //     vec3 specular     = numerator / max(denominator, 0.001);
    //
    //     // add to outgoing radiance Lo
    //     float NdotL = max(dot(N, L), 0.0);
    //     Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    //   }

    //sampel from spherical harmonic buffer precomputed
    vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec4 irrad_sample = texture(irradianceMap, gl_FragCoord.xy*invWindow);
    vec3 irradiance = irrad_sample.rgb;
    vec3 diffuse = irradiance * albedo;

    //sample from reflection buffer precomputed
   vec3 prefilteredColor = texture(prefilterMap, gl_FragCoord.xy*invWindow).rgb;
   vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
   vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
   float shadowfactor = (1 - ShadowCalculation(lightSpacePosition,dot(normal,normalize(shadowPos - outPosition)))*0.75);
    vec3 ambient = vec3(kD*diffuse + specular);//vec3(0.03) * albedo ;

    vec3 color = (ambient + Lo)*shadowfactor;
    // color = vec3(texture(brdfLUT,clamp(gl_FragCoord.xy,vec2(0),vec2(400))*(vec2(1.0/400))).rg,0.0);
    //color = normalize(outPosition - eyePos)*vec3(0.5) + vec3(0.5);//texture(cmap,normalize(outPosition - eyePos)*vec3(-1,1,1)).rgb;
    fragColor = vec4(color*when_gt(irrad_sample.a,1), 1.0);
//  fragColor = vec4(0,0,0,1);
}
