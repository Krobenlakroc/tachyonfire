#include "th_splineutils.h"
#include <stdio.h>
 float GetT( float t, float alpha, fn_vec3  p0, fn_vec3 p1 )
{
    fn_vec3 d  = fn_subVec3(p1 , p0);
    float a = fn_dot(d,d); // Dot product
    float b = powf( a, alpha*0.5f );
    return (b + t);
}

 fn_vec3 CatmullRom( fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{
  float alpha = 0.5;
    float t0 = 0.0f;
    float t1 = GetT( t0, alpha, p0, p1 );
    float t2 = GetT( t1, alpha, p1, p2 );
    float t3 = GetT( t2, alpha, p2, p3 );
    t = fn_lerp( t1, t2, t );
    fn_vec3 A1 = fn_addVec3(fn_multVec3s(p0,( t1-t )/( t1-t0 )) , fn_multVec3s(p1,( t-t0 )/( t1-t0 )));
    fn_vec3 A2 = fn_addVec3(fn_multVec3s(p1,( t2-t )/( t2-t1 )) , fn_multVec3s(p2,( t-t1 )/( t2-t1 )));
    fn_vec3 A3 = fn_addVec3(fn_multVec3s(p2,( t3-t )/( t3-t2 )) , fn_multVec3s(p3,( t-t2 )/( t3-t2 )));
    fn_vec3 B1 = fn_addVec3(fn_multVec3s(A1,( t2-t )/( t2-t0 )) , fn_multVec3s(A2,( t-t0 )/( t2-t0 )));
    fn_vec3 B2 = fn_addVec3(fn_multVec3s(A2,( t3-t )/( t3-t1 )) , fn_multVec3s(A3,( t-t1 )/( t3-t1 )));
    fn_vec3 C  = fn_addVec3(fn_multVec3s(B1,( t2-t )/( t2-t1 )) , fn_multVec3s(B2,( t-t1 )/( t2-t1 )));
    return C;
}


 fn_mat4 th_6dofCamera(fn_vec3* old_forward,fn_vec3* old_right,fn_vec3* old_up,fn_vec3 new_forward,fn_vec3 pos)
{
  fn_vec3 f;
  fn_vec3 s;
  fn_vec3 u;

  f = new_forward;
  f = fn_normalizeVec3(f);

  s = fn_cross(f, *old_up);
  s = fn_normalizeVec3(s);

  u = fn_cross(s, f);

  fn_mat4 pOut;
  pOut.m[0] = s.x;
  pOut.m[1] = u.x;
  pOut.m[2] = -f.x;
  pOut.m[3] = 0.0;

  pOut.m[4] = s.y;
  pOut.m[5] = u.y;
  pOut.m[6] = -f.y;
  pOut.m[7] = 0.0;

  pOut.m[8] = s.z;
  pOut.m[9] = u.z;
  pOut.m[10] = -f.z;
  pOut.m[11] = 0.0;

  pOut.m[12] = -fn_dot(s, pos);
  pOut.m[13] = -fn_dot(u, pos);
  pOut.m[14] = fn_dot(f, pos);
  pOut.m[15] = 1.0;
  *old_up = u;
  if (old_right != NULL)
  {
    *old_right = s;
  }
  if (old_forward != NULL)
  {
    *old_forward = f;
  }


  return pOut;
}

 fn_vec3 CalculateTangentUnNorm(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
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

 fn_vec3 CalculateAccel(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{

      fn_vec3 start,end,tanPoint1,tanPoint2;
      start = p1;
      end = p2;



          tanPoint1 = fn_subVec3(end , p0);

          tanPoint2 = fn_subVec3(p3 , start);
      fn_multVec3s(tanPoint1,0.5);
      fn_multVec3s(tanPoint2,0.5);


        fn_vec3 t1 = fn_multVec3s(start,(12*t - 6) );
        fn_vec3 t2 = fn_multVec3s(tanPoint1,(6*t - 4));
        fn_vec3 t3 = fn_multVec3s(end,(-12*t + 6));
        fn_vec3 t4 = fn_multVec3s(tanPoint2,(6*t - 2));

        fn_vec3 tangent = fn_addVec3(t1,fn_addVec3(t2,fn_addVec3(t3,t4)));

        return tangent;
}

 fn_vec3 CalculateJerk(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{

      fn_vec3 start,end,tanPoint1,tanPoint2;
      start = p1;
      end = p2;



          tanPoint1 = fn_subVec3(end , p0);

          tanPoint2 = fn_subVec3(p3 , start);
      fn_multVec3s(tanPoint1,0.5);
      fn_multVec3s(tanPoint2,0.5);


        fn_vec3 t1 = fn_multVec3s(start,(12 ) );
        fn_vec3 t2 = fn_multVec3s(tanPoint1,(6 ));
        fn_vec3 t3 = fn_multVec3s(end,(-12 ));
        fn_vec3 t4 = fn_multVec3s(tanPoint2,(6));

        fn_vec3 tangent = fn_addVec3(t1,fn_addVec3(t2,fn_addVec3(t3,t4)));

        return tangent;
}

 fn_vec3 CalculateTangent(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
{

        return fn_normalizeVec3(CalculateTangentUnNorm(p0,p1,p2,p3,t));
}

 fn_vec3  CalculatePosition(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */)
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

 float  CalculatePosition1D(float p0, float p1, float p2, float p3, float t /* between 0 and 1 */)
{
  float start,end,tanPoint1,tanPoint2;
  start = p1;
  end = p2;



  tanPoint1 = (end - p0);

  tanPoint2 = (p3 - start);
  (tanPoint1 *= 0.5);
  (tanPoint2 *= 0.5);


  float t1 = (start*(2.0f * t * t * t - 3.0f * t * t + 1.0f));
  float t2 = (tanPoint1*(t * t * t - 2.0f * t * t + t) );
  float t3 = (end*(-2.0f * t * t * t + 3.0f * t * t));
  float t4 = (tanPoint2*(t * t * t - t * t));

  float position = t1 + t2 + t3 + t4;
  return position;
}

 void computeUpVectors(fn_vec3* course,fn_vec3* upvectors,fn_vec3 first_up,fn_vec3 first_side,fn_vec3 first_forward,int count_course)
{
  fn_vec3 old_up = first_up;
  fn_vec3 old_right = first_side;
  fn_vec3 old_forward = first_forward;
  int cstart = 0;
  float interp = 0;
  upvectors[0] = first_up;
  for (int i = 0 ; i < (count_course - 1)*2;i++)
  {
    interp = (0.5)*((i + 1)%2);

    // if(cstart > count_course || cstart + 1 > count_course || cstart + 2 > count_course || cstart + 3 > count_course)
    // {
    //   printf("%s %i %i %i\n","OVERREACH",i,cstart,count_course );
    // }

    fn_vec3 pos;
    fn_vec3 forward;

    if (cstart == count_course - 1)
    {
      pos = CalculatePosition(course[cstart - 3],course[cstart - 2],course[cstart - 1],course[cstart],interp);
      forward = CalculateTangent(course[cstart - 3],course[cstart - 2],course[cstart - 1],course[cstart],interp);
    }
    else if (cstart == count_course - 2)
    {
      pos = CalculatePosition(course[cstart - 2],course[cstart - 1],course[cstart],course[cstart + 1],interp);
      forward = CalculateTangent(course[cstart - 2],course[cstart - 1],course[cstart],course[cstart + 1],interp);
    }
    else if (cstart == count_course - 3)
    {
      pos = CalculatePosition(course[cstart - 1],course[cstart],course[cstart + 1],course[cstart + 2],interp);
      forward = CalculateTangent(course[cstart - 1],course[cstart],course[cstart + 1],course[cstart + 2],interp);
    }
    else
    {
      pos = CalculatePosition(course[cstart],course[cstart + 1],course[cstart + 2],course[cstart + 3],interp);
      forward = CalculateTangent(course[cstart],course[cstart + 1],course[cstart + 2],course[cstart + 3],interp);
    }

    th_6dofCamera(&old_forward,&old_right,&old_up,forward,pos);
    upvectors[i + 1] = old_up;
    cstart += ((i + 1)%2);
  }
}
