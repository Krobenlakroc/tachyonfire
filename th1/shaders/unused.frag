
void mult2(
    out Light y,
    in Light f,
    in vec4 g)
{

    y = f;

    vec3 tf, tg, t;
    // [0,0]: 0,
    y.a[0] = vec3(0.282094792935999980)*f.a[0] * g.x;

    // [1,1]: 0,
    tf = vec3(0.282094791773000010)*f.a[0];
    tg = vec3(0.282094791773000010)*g.x;
    y.a[1] = tf*g.y + tg*f.a[1];
    t = f.a[1] * g.y;
    y.a[0] += vec3(0.282094791773000010)*t;

    // [2,2]: 0,
    tf = vec3(0.282094795249000000)*f.a[0];
    tg = vec3(0.282094795249000000)*g.x;
    y.a[2] = tf*g.z + tg*f.a[2];
    t = f.a[2] * g.z;
    y.a[0] += vec3(0.282094795249000000)*t;

    // [3,3]: 0,
    tf = vec3(0.282094791773000010)*f.a[0];
    tg = vec3(0.282094791773000010)*g.x;
    y.b[0] = tf*g.w + tg*f.b[0];
    t = f.b[0] * g.w;
    y.a[0] += vec3(0.282094791773000010)*t;

}

void mult3(
    out Light y,
    in Light f,
    in Light g)
{


    vec3 tf, tg, t;
    // [0,0]: 0,
    y.a[0] = vec3(0.282094792935999980)*f.a[0] * g.a[0];

    // [1,1]: 0,6,8,
    tf = vec3(0.282094791773000010)*f.a[0] + vec3(-0.126156626101000010)*f.c[0] + vec3(-0.218509686119999990)*f.c[2];
    tg = vec3(0.282094791773000010)*g.a[0];
    y.a[1] = tf*g.a[1] + tg*f.a[1];
    t = f.a[1] * g.a[1];
    y.a[0] += vec3(0.282094791773000010)*t;
    y.c[0] = vec3(-0.126156626101000010)*t;
    y.c[2] = vec3(-0.218509686119999990)*t;

    // [1,2]: 5,
    tf = vec3(0.218509686118000010)*f.b[2];
    y.a[1] += tf*g.a[2];
    y.a[2] = tf*g.a[1];
    t = f.a[1] * g.a[2] + f.a[2] * g.a[1];
    y.b[2] = vec3(0.218509686118000010)*t;

    // [1,3]: 4,
    tf = vec3(0.218509686114999990)*f.b[1];
    y.a[1] += tf*g.b[0];
    y.b[0] = tf*g.a[1];
    t = f.a[1] * g.b[0] + f.b[0] * g.a[1];
    y.b[1] = vec3(0.218509686114999990)*t;

    // [2,2]: 0,6,
    tf = vec3(0.282094795249000000)*f.a[0] + vec3(0.252313259986999990)*f.c[0];
  //  tg = vec3(0.282094795249000000)*g.a[0];
    y.a[2] += tf*g.a[2] + tg*f.a[2];
    t = f.a[2] * g.a[2];
    y.a[0] += vec3(0.282094795249000000)*t;
    y.c[0] += vec3(0.252313259986999990)*t;

    // [2,3]: 7,
    tf = vec3(0.218509686118000010)*f.c[1];
    y.a[2] += tf*g.b[0];
    y.b[0] += tf*g.a[2];
    t = f.a[2] * g.b[0] + f.b[0] * g.a[2];
    y.c[1] = vec3(0.218509686118000010)*t;

    // [3,3]: 0,6,8,
    tf = vec3(0.282094791773000010)*f.a[0] + vec3(-0.126156626101000010)*f.c[0] + vec3(0.218509686119999990)*f.c[2];
  //  tg = vec3(0.282094791773000010)*g.a[0];
    y.b[0] += tf*g.b[0] + tg*f.b[0];
    t = f.b[0] * g.b[0];
    y.a[0] += vec3(0.282094791773000010)*t;
    y.c[0] += vec3(-0.126156626101000010)*t;
    y.c[2] += vec3(0.218509686119999990)*t;

    // [4,4]: 0,6,
  //  tf = vec3(0.282094791770000020)*f.a[0] + vec3(-0.180223751576000010)*f.c[0];
  //  tg = vec3(0.282094791770000020)*g.a[0];
    y.b[1] += tg*f.b[1];


    // [4,5]: 7,
    // tf = vec3(0.156078347226000000)*f.c[1];
    // y.b[1] += tf*0;
    // y.b[2] += tf*0;


    // [5,5]: 0,6,8,
  //  tf = vec3(0.282094791773999990)*f.a[0] + vec3(0.090111875786499998)*f.c[0] + vec3(-0.156078347227999990)*f.c[2];
    // tg = vec3(0.282094791773999990)*g.a[0];
    y.b[2] += tg*f.b[2];


    // [6,6]: 0,6,
    // tg = vec3(0.282094797560000000)*g.a[0];
    y.c[0] +=  tg*f.c[0];


    // [7,7]: 0,6,8,
    // tf = vec3(0.282094791773999990)*f.a[0] + vec3(0.090111875786499998)*f.c[0] + vec3(0.156078347227999990)*f.c[2];
    // tg = vec3(0.282094791773999990)*g.a[0];
    y.c[1] +=  tg*f.c[1];


    // [8,8]: 0,6,
    // tg = vec3(0.282094791770000020)*g.a[0];
    y.c[2] += tg*f.c[2];


    // multiply count=120

}

//max(0,dot(normalize(pos-worldpos),normal))*
// if (outTexcoord.x < 0.5)
// {
// vec4 t3 = texture(occTex,outTexcoord);
// vec4 c0123 = vec4(unpack_2half(t3.x).xy,unpack_2half(t3.y).xy);
// vec4 c4567 = vec4(unpack_2half(t3.z).xy,unpack_2half(t3.w).xy);
// vec3 n = normalize(pos-worldpos);
// return 1 - clamp(c0123.w +
//   c0123.x  * n.x +
//   c0123.y  * n.y +
//   c0123.z  * n.z +
//   c4567.x  * n.x*n.z +
//   c4567.y  * n.y*n.z +
//   c4567.z  * n.y*n.x +
//   c4567.w  * (3.0*n.z*n.z - 1.0),0,1);
// #ifdef OCCLUSION
#ifdef OCCLUSION
//  const vec4 sh2_weight = vec4(vec3(0.48860), 0.28209);
//     return (1- clamp(dot(vec4(normalize(pos-worldpos),1), sh*sh2_weight),0,1));
// #else
const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
 return (1 - clamp(dot(vec4(normalize(pos-worldpos),1).yzxw, sh*sh2_weight),0,1));
 #else
#endif
// #else
  // return 1;
// #endif
// }
// else
// {
//   return 1;
// }
// else
// {
  //return max(0,dot(normalize(pos-worldpos),normal));
//  }
//
//   vec4 t3 = texture(occTex,outTexcoord);
//   vec4 c0123 = vec4(unpack_2half(t3.x).xy,unpack_2half(t3.y).xy);
//   vec4 c4567 = vec4(unpack_2half(t3.z).xy,unpack_2half(t3.w).xy);
//   vec3 n = normalize(pos-worldpos);
//   return 1 - clamp(c0123.w +
//     c0123.x  * n.x +
//     c0123.y  * n.y +
//     c0123.z  * n.z ,0,1);
//     //return max(0,dot(normalize(pos-worldpos),normal));//*(1 - clamp(dot(vec4(normalize(pos-worldpos),1), texture(occTex,outTexcoord)),0,1));
// }

Light lerpLight(Light a,Light b,float m)
{
  a.a = a.a*(1 - m) + b.a*m;
  a.b = a.b*(1 - m) + b.b*m;
  a.c = a.c*(1 - m) + b.c*m;
  return a;
}

vec2  unpack_2half(float a)
{
  return (unpackHalf2x16(floatBitsToUint(a)));
}


float compress2_range(vec2 i)
{
  return 0.25*(i.x * pow(2,8) + i.y);
}

vec2 extract2_range(float i)
{
  i = i*4;
  return vec2(floor(i / pow(2,8)),mod(i,pow(2,8)));
}

#ifdef OCCLUSION
const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
float oc = (1 - clamp(dot(vec4(normalize(normal),1).yzxw, sh*sh2_weight),0,1));
#else
float oc =1;
 #endif
