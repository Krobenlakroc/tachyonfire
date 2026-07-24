#include "fn_spline.h"
#include <stdlib.h>
static fn_vec3 CalculateTangentUnNorm(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{

      fn_vec3 start,end,tanPoint1,tanPoint2;
      start = p1;
      end = p2;



          tanPoint1 = fn_subVec3(end , p0);

          tanPoint2 = fn_subVec3(p3 , start);
      fn_multVec3s(tanPoint1,0.5);
      fn_multVec3s(tanPoint2,0.5);


        fn_vec3 t1 = fn_multVec3s(start,(6 * t * t - 6 * t) );
        fn_vec3 t2 = fn_multVec3s(tanPoint1,(3 * t * t - 4 * t + 1));
        fn_vec3 t3 = fn_multVec3s(end,(-6 * t * t + 6 * t));
        fn_vec3 t4 = fn_multVec3s(tanPoint2,(3 * t * t - 2 * t));

        fn_vec3 tangent = fn_addVec3(t1,fn_addVec3(t2,fn_addVec3(t3,t4)));

        return tangent;
}


static fn_vec3 CalculateTangent(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{

        return fn_normalizeVec3(CalculateTangentUnNorm(p0,p1,p2,p3,t));
}

static fn_vec3  CalculatePosition(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{
  fn_vec3 start,end,tanPoint1,tanPoint2;
  start = p1;
  end = p2;



  tanPoint1 = fn_subVec3(end , p0);

  tanPoint2 = fn_subVec3(p3 , start);
  fn_multVec3s(tanPoint1,0.5);
  fn_multVec3s(tanPoint2,0.5);


  fn_vec3 t1 = fn_multVec3s(start,(2.0f * t * t * t - 3.0f * t * t + 1.0f));
  fn_vec3 t2 = fn_multVec3s(tanPoint1,(t * t * t - 2.0f * t * t + t) );
  fn_vec3 t3 = fn_multVec3s(end,(-2.0f * t * t * t + 3.0f * t * t));
  fn_vec3 t4 = fn_multVec3s(tanPoint2,(t * t * t - t * t));

  fn_vec3 position = fn_addVec3(t1,fn_addVec3(t2,fn_addVec3(t3,t4)));
  return position;
}

static fn_vec3 target_point;

fn_mat4 fn_splineCamera(float dt,fn_SplineState* info)
{

  fn_vec3 d = CalculateTangentUnNorm(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
  float speed = dt*info->speed;//dt*0.7;
  float interp_delta = speed / sqrtf( d.x*d.x + d.y*d.y + d.z*d.z );
  info->interp += interp_delta;
  if (info->cprog == 0)
  {
    info->forward = CalculateTangent(info->course[0],info->course[0 + 1],info->course[0 + 2],info->course[0 + 3],info->interp);
  }
  if (info->interp > 1)
  {
    info->cprog++;
    info->interp = info->interp - 1;
    if (info->cprog > info->course_count - 4)
    {
      //info->cprog = 0;
    info->cprog--;
    info->interp = info->interp + 1;
    info->interp -= interp_delta;
    }
  }

  int cstart = info->cprog;
  float interp = info->interp;
  fn_vec3 pos = CalculatePosition(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
  fn_vec3 forward_target = CalculateTangent(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
  //pos = fn_multVec3s(pos,-1);
  info->pos = pos;
  //fn_createVec3(1,0,-1);
  //
  if (info->mode == SPLINE_LOOKAT)
  {
    info->forward = fn_subVec3(info->targetpos,pos);
  }
  else if (info->mode == SPLINE_FORWARD)
  {
    info->forward =  fn_slerpVec3(info->forward,forward_target,1*dt*0.001);
  }
  else if (info->mode == SPLINE_CONSTDIR)
  {
    info->forward =  info->targetpos;//fn_slerpVec3(info->forward,forward_target,1*dt*0.001);
  }

  fn_mat4 view =  (fn_lookat(fn_multVec3(pos,fn_createVec3(-1,1,1)),fn_addVec3(fn_multVec3(pos,fn_createVec3(-1,1,1)),fn_multVec3(info->forward,fn_createVec3(-1,1,1))),fn_createVec3(0,-1,0)));
  return fn_scale(view,fn_createVec3(-1,1,1));
}

void fn_defaultSplineState(fn_SplineState* st)
{

  // st->targetpos = fn_createVec3(0,0,-1);
  //
  // st->mode = SPLINE_CONSTDIR;

  st->speed = 0.4;
  st->interp = 0;
  st->cprog = 0;
  st->course_count = 100;
  st->course = malloc(sizeof(fn_vec3)*st->course_count);
  fn_vec3* course = st->course;

//LEVELDREAM
//   st->targetpos = fn_createVec3(176.134827,-811.526001,22.152098);
//
//   st->mode = SPLINE_LOOKAT;
//
// course[0]=fn_createVec3(-648.528381,-814.152649,319.824554);
// course[1]=fn_createVec3(-477.189209,-790.807678,477.385376);
// course[2]=fn_createVec3(-269.599274,-777.846313,643.197144);
// course[3]=fn_createVec3(-88.271240,-772.121155,685.041504);
// course[4]=fn_createVec3(149.295212,-760.947693,641.747070);
// course[5]=fn_createVec3(366.554932,-746.664917,565.139587);
// course[6]=fn_createVec3(653.432251,-746.345093,398.069305);
// course[7]=fn_createVec3(897.326843,-750.414246,248.932587);
// course[8]=fn_createVec3(1054.893433,-756.181274,34.926651);
// course[9]=fn_createVec3(1101.472534,-760.678528,-106.997925);
// course[10]=fn_createVec3(1058.096313,-765.265747,-252.358139);
// course[11]=fn_createVec3(1027.217896,-767.124634,-306.089691);
  // st->course_count = 12;

//OPENAIRE

// st->targetpos = fn_createVec3(-1035.239868,-385.612305,180.254990);
//
// st->mode = SPLINE_LOOKAT;
//
// course[0]=fn_createVec3(-1543.805298,-485.087372,133.459000);
// course[1]=fn_createVec3(-1310.230469,-594.252991,180.386185);
// course[2]=fn_createVec3(-1046.738647,-693.953247,189.775620);
// course[3]=fn_createVec3(-879.042786,-704.009216,188.767242);
// course[4]=fn_createVec3(-634.205688,-655.906494,185.503143);
// course[5]=fn_createVec3(-495.340851,-523.121643,167.426270);
// course[6]=fn_createVec3(-400.608124,-411.616028,122.351051);
// course[7]=fn_createVec3(-618.579285,-402.274780,182.575333);
//  st->course_count = 8;

//TITLELEVERL!
// float zp = -165;
// course[0]=fn_createVec3(1158.516968,-141.765381,zp);
// course[1]=fn_createVec3(707.873596,-141.765381,zp);
// course[2]=fn_createVec3(466.036591,-141.765381,zp);
// course[3]=fn_createVec3(244.163925,-141.765381,zp);
// course[4]=fn_createVec3(-11.559650,-141.765381,zp);
// course[5]=fn_createVec3(-193.461533,-141.765381,zp);
// course[6]=fn_createVec3(-404.101501,-141.765381,zp);
// course[7]=fn_createVec3(-596.803162,-141.765381,zp);
// course[8]=fn_createVec3(-813.486694,-141.765381,zp);
// st->targetpos = fn_createVec3(0,0,1);
//
// st->mode = SPLINE_CONSTDIR;
// st->course_count = 9;

//TITLELEVEL2
course[0]=fn_createVec3(89.287270,-154.912018,-27.216599);
course[1]=fn_createVec3(89.287270,-153.458893,-66.560165);
course[2]=fn_createVec3(89.287270,-154.738571,-130.544159);
course[3]=fn_createVec3(89.287270,-156.518066,-219.521957);
course[4]=fn_createVec3(89.287270,-159.097336,-348.489777);
course[5]=fn_createVec3(89.287270,-160.197037,-403.476044);
course[6]=fn_createVec3(89.287270,-163.476166,-468.459808);
course[7]=fn_createVec3(89.287270,-163.476166,-504.406433);
course[8]=fn_createVec3(89.287270,-163.476166,-567.435120);

// course[7]=fn_createVec3(122.917152,-164.735840,-630.419556);
// course[8]=fn_createVec3(121.937378,-166.695343,-532.443665);
// course[9]=fn_createVec3(121.317513,-167.935013,-470.459076);
st->targetpos = fn_createVec3(0,0,1);

st->mode = SPLINE_CONSTDIR;
st->speed = 0.03;
  st->course_count = 9;


}
