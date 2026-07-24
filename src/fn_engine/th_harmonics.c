#include "th_harmonics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "th_threads.h"
#include "th_renderer.h"
#include "../th_fopen.h"

static GLuint harmTex = 0;
static bool init_vram_harmtex = false;

GLuint th_createLightTexture(th_Harmonic* harmonics,int count,fn_vec2* outdims)
{


  int n;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &n);
  printf("%i\n",n );

  if (count > TH_HARMONICS_MAX_COUNT )
  {
    printf("Too many probes for texture\n");
    exit(0);
  }


  float dnum_max = (float)(TH_HARMONICS_MAX_COUNT*9 )/ (float)n;
  int rows_max = (int)ceil(dnum_max);

  int TH_HARMONICS_MAX_ROW = rows_max;

  float dnum = (float)(count*9 )/ (float)n;
  int rows = (int)ceil(dnum);

  float* image = malloc(sizeof(float)*3*(fn_clampi(count*9,0,n)*rows));//malloc(sizeof(float)*3*9*(fn_clampi(count*9,0,n)*rows));
   memset(image,0,sizeof(float)*3*(fn_clampi(count*9,0,n)*rows));
  int i;
  for (i =0 ; i < count;i++)
  {
    image[i*9*3 + 0] = harmonics[i].c0.x;
    image[i*9*3 + 1] = harmonics[i].c0.y;
    image[i*9*3 + 2] = harmonics[i].c0.z;

    image[i*9*3 + 3] = harmonics[i].c1.x;
    image[i*9*3 + 4] = harmonics[i].c1.y;
    image[i*9*3 + 5] = harmonics[i].c1.z;

    image[i*9*3 + 6] = harmonics[i].c2.x;
    image[i*9*3 + 7] = harmonics[i].c2.y;
    image[i*9*3 + 8] = harmonics[i].c2.z;

    image[i*9*3 + 9] = harmonics[i].c3.x;
    image[i*9*3 + 10] = harmonics[i].c3.y;
    image[i*9*3 + 11] = harmonics[i].c3.z;

    image[i*9*3 + 12] = harmonics[i].c4.x;
    image[i*9*3 + 13] = harmonics[i].c4.y;
    image[i*9*3 + 14] = harmonics[i].c4.z;

    image[i*9*3 + 15] = harmonics[i].c5.x;
    image[i*9*3 + 16] = harmonics[i].c5.y;
    image[i*9*3 + 17] = harmonics[i].c5.z;

    image[i*9*3 + 18] = harmonics[i].c6.x;
    image[i*9*3 + 19] = harmonics[i].c6.y;
    image[i*9*3 + 20] = harmonics[i].c6.z;

    image[i*9*3 + 21] = harmonics[i].c7.x;
    image[i*9*3 + 22] = harmonics[i].c7.y;
    image[i*9*3 + 23] = harmonics[i].c7.z;

    image[i*9*3 + 24] = harmonics[i].c8.x;
    image[i*9*3 + 25] = harmonics[i].c8.y;
    image[i*9*3 + 26] = harmonics[i].c8.z;

  }

  if (!init_vram_harmtex)
  {
    glGenTextures(1, &harmTex);
    glBindTexture(GL_TEXTURE_2D, harmTex);
    glTexStorage2D(GL_TEXTURE_2D,
                   1,
                   GL_RGB16F,
                   n,
                   TH_HARMONICS_MAX_ROW);

    glClearTexImage(harmTex,
                    0,
                    GL_RGB,
                    GL_FLOAT,
                    NULL);

    init_vram_harmtex = true;
  }
  else
  {
    glBindTexture(GL_TEXTURE_2D, harmTex);
  }




  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, fn_clampi(count*9,0,n),rows,0, GL_RGB, GL_FLOAT, image);

  glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                   0,
                   0,
                   fn_clampi(count*9,0,n),
                   rows,
                   GL_RGB,
                   GL_FLOAT,
                   image);

  //fn_clampi(count*9,0,n)
  *outdims = fn_createVec2(n,TH_HARMONICS_MAX_ROW);//TH_HARMONICS_MAX_ROW
  // printf("%f %f\n",outdims->x,outdims->y );

  // texture sampling/filtering operation.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  free(image);
  return harmTex;
}
#define PI 3.14159265359
float areaElement (float x, float y)
{
  return atan2(x * y, sqrt(x * x + y * y + 1.0));
}

float texelSolidAngle (float aU,float aV, float width,float height) {
  // transform from [0..res - 1] to [- (1 - 1 / res) .. (1 - 1 / res)]
  // ( 0.5 is for texel center addressing)
  float U = (2.0 * (aU + 0.5) / width) - 1.0;
  float V = (2.0 * (aV + 0.5) / height) - 1.0;

  // shift from a demi texel, mean 1.0 / size  with U and V in [-1..1]
  float invResolutionW = 1.0 / width;
  float invResolutionH = 1.0 / height;

  // U and V are the -1..1 texture coordinate on the current face.
  // get projected area for this texel
  float x0 = U - invResolutionW;
  float y0 = V - invResolutionH;
  float x1 = U + invResolutionW;
  float y1 = V + invResolutionH;
  float angle = areaElement(x0, y0) - areaElement(x0, y1) - areaElement(x1, y0) + areaElement(x1, y1);

  return angle;
}


void getSHBasis(fn_vec3 n,float *b)
{
  float x = n.x;
  float y = n.y;
  float z = n.z;
  const double z2 = z*z;


  /* m=0 */

  // l=0
  const double p_0_0 = (0.282094791773878140);
  b[0] = p_0_0; // l=0,m=0
  // l=1
  const double p_1_0 = (0.488602511902919920)*z;
  b[2] = p_1_0; // l=1,m=0
  // l=2
  const double p_2_0 = (0.946174695757560080)*z2 + (-0.315391565252520050);
  b[6] = p_2_0; // l=2,m=0


  /* m=1 */

  const double s1 = y;
  const double c1 = x;

  // l=1
  const double p_1_1 = (-0.488602511902919920);
  b[1] = p_1_1*s1; // l=1,m=-1
  b[3] = p_1_1*c1; // l=1,m=+1
  // l=2
  const double p_2_1 = (-1.092548430592079200)*z;
  b[5] = p_2_1*s1; // l=2,m=-1
  b[7] = p_2_1*c1; // l=2,m=+1


  /* m=2 */

  const double s2 = x*s1 + y*c1; //xy + xy
  const double c2 = x*c1 - y*s1; //x^2 - y^2

  // l=2
  const double p_2_2 = (0.546274215296039590);
  b[4] = p_2_2*s2; // l=2,m=-2
  b[8] = p_2_2*c2; // l=2,m=+2
}

typedef struct
{
  int start;
  int range;
  th_Harmonic* ret;
  float* weightAccum;
  GLfloat** faces;
  int dimension;
  fn_vec3** cubemapFaceNormals;
}th_CubemapHarmonicData;

void th_cubeharmonicsthread(void* data)
{
  th_CubemapHarmonicData* d = (th_CubemapHarmonicData*)data;

  fn_vec3** cubemapFaceNormals = d->cubemapFaceNormals;
  GLfloat** faces = d->faces;
  int dimension = d->dimension;

  th_Harmonic ret = *d->ret;
  float weightAccum = *d->weightAccum;

  for (int c = d->start;c < d->start + d->range;c++)
  {
    int i = c/(dimension*dimension);
    GLfloat* face = d->faces[c/(dimension*dimension)];
    int j = c % (dimension*dimension);

      fn_vec3 color = fn_createVec3(face[j*3 + 0],face[j*3 + 1],face[j*3 + 2]);
      float x = (j % dimension);
      float y = (j - x)/dimension;
      float fU = (2.0 * x / (dimension - 1.0)) - 1.0;
      float fV = (2.0 * y / (dimension - 1.0)) - 1.0;

    fn_vec3 vecx = fn_multVec3s(cubemapFaceNormals[i][0],fU);

    fn_vec3 vecy = fn_multVec3s(cubemapFaceNormals[i][1],fV);
    fn_vec3 vecz = cubemapFaceNormals[i][2];

    fn_vec3 norm = fn_addVec3(vecx,vecy);
    norm = fn_addVec3(norm,vecz);
    norm = fn_normalizeVec3(norm);
    norm = fn_multVec3(norm,fn_createVec3(-1,1,1));

    float weight = texelSolidAngle(x,y,dimension,dimension);

    weightAccum += weight;

    float basis[9];
    getSHBasis(norm,basis);
    ret.c0 = fn_addVec3(ret.c0,fn_multVec3s(color,basis[0]*weight));
    ret.c1 = fn_addVec3(ret.c1,fn_multVec3s(color,basis[1]*weight));
    ret.c2 = fn_addVec3(ret.c2,fn_multVec3s(color,basis[2]*weight));
    ret.c3 = fn_addVec3(ret.c3,fn_multVec3s(color,basis[3]*weight));
    ret.c4 = fn_addVec3(ret.c4,fn_multVec3s(color,basis[4]*weight));
    ret.c5 = fn_addVec3(ret.c5,fn_multVec3s(color,basis[5]*weight));
    ret.c6 = fn_addVec3(ret.c6,fn_multVec3s(color,basis[6]*weight));
    ret.c7 = fn_addVec3(ret.c7,fn_multVec3s(color,basis[7]*weight));
    ret.c8 = fn_addVec3(ret.c8,fn_multVec3s(color,basis[8]*weight));

  }

  *d->ret = ret;
  *d->weightAccum = weightAccum;
}

th_Harmonic th_CubemaptoHarmonic(GLfloat** faces,int dimension)
{
  // posx
// negx
// posy
// negy
// posz
// negz
  static fn_vec3** cubemapFaceNormals = NULL;//[6][3];

  static th_Harmonic* rets = NULL;

  static float* weightAccums = NULL;

  if (cubemapFaceNormals == NULL)
  {
    rets = malloc(sizeof(th_Harmonic)*th_getNumThreads());
    weightAccums = malloc(sizeof(float)*th_getNumThreads());
    cubemapFaceNormals = malloc(sizeof(fn_vec3*)*6);
    for (size_t i = 0; i < 6; i++) {
      cubemapFaceNormals[i] = malloc(sizeof(fn_vec3)*3);
    }

    cubemapFaceNormals[0][0]=fn_createVec3(0,0,-1);cubemapFaceNormals[0][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[0][2]=fn_createVec3(1,0,0);
    cubemapFaceNormals[1][0]=fn_createVec3(0,0,1);cubemapFaceNormals[1][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[1][2]=fn_createVec3(-1,0,0);
    cubemapFaceNormals[2][0]=fn_createVec3(1,0,0);cubemapFaceNormals[2][1]=fn_createVec3(0,0,1);cubemapFaceNormals[2][2]=fn_createVec3(0,1,0);
    cubemapFaceNormals[3][0]=fn_createVec3(1,0,0);cubemapFaceNormals[3][1]=fn_createVec3(0,0,-1);cubemapFaceNormals[3][2]=fn_createVec3(0,-1,0);
    cubemapFaceNormals[4][0]=fn_createVec3(1,0,0);cubemapFaceNormals[4][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[4][2]=fn_createVec3(0,0,1);
    cubemapFaceNormals[5][0]=fn_createVec3(-1,0,0);cubemapFaceNormals[5][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[5][2]=fn_createVec3(0,0,-1);
  }




  th_Harmonic ret;
  ret.c0 = fn_createVec3s(0);
  ret.c1 = fn_createVec3s(0);
  ret.c2 = fn_createVec3s(0);
  ret.c3 = fn_createVec3s(0);
  ret.c4 = fn_createVec3s(0);
  ret.c5 = fn_createVec3s(0);
  ret.c6 = fn_createVec3s(0);
  ret.c7 = fn_createVec3s(0);
  ret.c8 = fn_createVec3s(0);
  float weightAccum = 0;


  for (int i = 0; i < th_getNumThreads(); i++) {
    rets[i] = ret;
    weightAccums[i] = 0;
  }

  TH_BEGIN_SCHEDULING((uint)th_getNumThreads(),th_CubemapHarmonicData,dimension*dimension*6)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].ret = &rets[th_thread_id];
  data[th_thread_id].weightAccum = &weightAccums[th_thread_id];
  data[th_thread_id].faces = faces;
  data[th_thread_id].dimension = dimension;
  data[th_thread_id].cubemapFaceNormals = cubemapFaceNormals;
  TH_SCHEDULING_FUNC
  th_setThread(th_cubeharmonicsthread,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING

  for (int i = 0; i < th_getNumThreads(); i++) {
    ret.c0 = fn_addVec3(ret.c0,rets[i].c0);
    ret.c1 = fn_addVec3(ret.c1,rets[i].c1);
    ret.c2 = fn_addVec3(ret.c2,rets[i].c2);
    ret.c3 = fn_addVec3(ret.c3,rets[i].c3);
    ret.c4 = fn_addVec3(ret.c4,rets[i].c4);
    ret.c5 = fn_addVec3(ret.c5,rets[i].c5);
    ret.c6 = fn_addVec3(ret.c6,rets[i].c6);
    ret.c7 = fn_addVec3(ret.c7,rets[i].c7);
    ret.c8 = fn_addVec3(ret.c8,rets[i].c8);
    weightAccum = weightAccum + weightAccums[i];
  }

  // for (int i = 0;i < 6;i++)
  // {
  //   GLfloat* face = faces[i];
  //   for (int j = 0 ; j < dimension*dimension;j++)
  //   {
  //     fn_vec3 color = fn_createVec3(face[j*3 + 0],face[j*3 + 1],face[j*3 + 2]);
  //     float x = (j % dimension);
  //     float y = (j - x)/dimension;
  //     float fU = (2.0 * x / (dimension - 1.0)) - 1.0;
  //     float fV = (2.0 * y / (dimension - 1.0)) - 1.0;
  //
  //   fn_vec3 vecx = fn_multVec3s(cubemapFaceNormals[i][0],fU);
  //
  //   fn_vec3 vecy = fn_multVec3s(cubemapFaceNormals[i][1],fV);
  //   fn_vec3 vecz = cubemapFaceNormals[i][2];
  //
  //   fn_vec3 norm = fn_addVec3(vecx,vecy);
  //   norm = fn_addVec3(norm,vecz);
  //   norm = fn_normalizeVec3(norm);
  //   norm = fn_multVec3(norm,fn_createVec3(-1,1,1));
  //
  //   float weight = texelSolidAngle(x,y,dimension,dimension);
  //
  //   weightAccum += weight;
  //
  //   float basis[9];
  //   getSHBasis(norm,basis);
  //   ret.c0 = fn_addVec3(ret.c0,fn_multVec3s(color,basis[0]*weight));
  //   ret.c1 = fn_addVec3(ret.c1,fn_multVec3s(color,basis[1]*weight));
  //   ret.c2 = fn_addVec3(ret.c2,fn_multVec3s(color,basis[2]*weight));
  //   ret.c3 = fn_addVec3(ret.c3,fn_multVec3s(color,basis[3]*weight));
  //   ret.c4 = fn_addVec3(ret.c4,fn_multVec3s(color,basis[4]*weight));
  //   ret.c5 = fn_addVec3(ret.c5,fn_multVec3s(color,basis[5]*weight));
  //   ret.c6 = fn_addVec3(ret.c6,fn_multVec3s(color,basis[6]*weight));
  //   ret.c7 = fn_addVec3(ret.c7,fn_multVec3s(color,basis[7]*weight));
  //   ret.c8 = fn_addVec3(ret.c8,fn_multVec3s(color,basis[8]*weight));
  //   }
  // }

  ret.c0 = fn_multVec3s(ret.c0,4.f * PI / weightAccum);
  ret.c1 = fn_multVec3s(ret.c1,4.f * PI / weightAccum);
  ret.c2 = fn_multVec3s(ret.c2,4.f * PI / weightAccum);
  ret.c3 = fn_multVec3s(ret.c3,4.f * PI / weightAccum);
  ret.c4 = fn_multVec3s(ret.c4,4.f * PI / weightAccum);
  ret.c5 = fn_multVec3s(ret.c5,4.f * PI / weightAccum);
  ret.c6 = fn_multVec3s(ret.c6,4.f * PI / weightAccum);
  ret.c7 = fn_multVec3s(ret.c7,4.f * PI / weightAccum);
  ret.c8 = fn_multVec3s(ret.c8,4.f * PI / weightAccum);


  //convoltuiion
  ret.c0 = fn_multVec3s(ret.c0,PI*(1.0));
  ret.c1 = fn_multVec3s(ret.c1,PI*(2.0/3.0));
  ret.c2 = fn_multVec3s(ret.c2,PI*(2.0/3.0));
  ret.c3 = fn_multVec3s(ret.c3,PI*(2.0/3.0));
  ret.c4 = fn_multVec3s(ret.c4,PI*(1.0/4.0));
  ret.c5 = fn_multVec3s(ret.c5,PI*(1.0/4.0));
  ret.c6 = fn_multVec3s(ret.c6,PI*(1.0/4.0));
  ret.c7 = fn_multVec3s(ret.c7,PI*(1.0/4.0));
  ret.c8 = fn_multVec3s(ret.c8,PI*(1.0/4.0));

  // printf("%f\n", 4.f * PI / weightAccum);
  return ret;
}
// float weight1 =0.282095*weight;//*PI;
// float weight2 =0.488603*weight;//*((2*PI)/3);
// float weight3 =1.092548*weight;//*(PI/4);
// float weight4 = 0.315392*weight;//*(PI/4);
// float weight5 = 0.546274*weight;//*(PI/4);
// float weight1 = (4.f/17)*weight;
// float weight2 = (8.f/17)*weight;
// float weight3 = (15.f/17)*weight;
// float weight4 = (5.f/68)*weight;
// float weight5 = (15.f/68)*weight;
// ret.c0 = fn_addVec3(ret.c0,fn_multVec3s(color,weight1*1));
// ret.c1 = fn_addVec3(ret.c1,fn_multVec3s(color,weight2*norm.x));
// ret.c2 = fn_addVec3(ret.c2,fn_multVec3s(color,weight2*norm.y));
// ret.c3 = fn_addVec3(ret.c3,fn_multVec3s(color,weight2*norm.z));
// ret.c4 = fn_addVec3(ret.c4,fn_multVec3s(color,weight3*norm.x*norm.z));
// ret.c5 = fn_addVec3(ret.c5,fn_multVec3s(color,weight3*norm.z*norm.y));
// ret.c6 = fn_addVec3(ret.c6,fn_multVec3s(color,weight3*norm.x*norm.y));
// ret.c7 = fn_addVec3(ret.c7,fn_multVec3s(color,weight4*(3*norm.z*norm.z - 1)));
// ret.c8 = fn_addVec3(ret.c8,fn_multVec3s(color,weight5*(norm.x*norm.x - norm.y*norm.y)));



void th_exportHarmonicsBinary(th_Harmonic* harmonics,int count,const char* filename)
{
  FILE* mwad = th_fopen(filename,"wb");
  if (mwad == NULL)
  {
    printf("%s\n","CANNOT OPEN" );
  }

  // fwrite(harmonics,sizeof(th_Harmonic),count,mwad);
  for (int i = 0 ; i < count;i++)
  {
    fwrite(harmonics[i].c0.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c1.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c2.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c3.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c4.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c5.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c6.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c7.v,sizeof(float),3,mwad);
    fwrite(harmonics[i].c8.v,sizeof(float),3,mwad);
  }

  fclose(mwad);
}

th_Harmonic* th_loadHarmonicsBinary(th_Allocator* alloc,char* filename, int* count,fn_vec3 griddims,fn_vec3 gridpos,float gridsize)
{
  int gdx = griddims.x;
  int gdy = griddims.y;
  int gdz = griddims.z;
  int count_gdims = gdx*gdy*gdz;

  th_Harmonic* harmonics = th_alloc(alloc,sizeof(th_Harmonic)*count_gdims);
  *count = count_gdims;

  FILE* mwad = th_fopen(filename,"rb");
  if (mwad == NULL)
  {
    printf("%s\n","CANNOT OPEN" );
  }
  for (int i = 0 ; i < *count;i++)
  {
    fread(harmonics[i].c0.v,sizeof(float),3,mwad);
    fread(harmonics[i].c1.v,sizeof(float),3,mwad);
    fread(harmonics[i].c2.v,sizeof(float),3,mwad);
    fread(harmonics[i].c3.v,sizeof(float),3,mwad);
    fread(harmonics[i].c4.v,sizeof(float),3,mwad);
    fread(harmonics[i].c5.v,sizeof(float),3,mwad);
    fread(harmonics[i].c6.v,sizeof(float),3,mwad);
    fread(harmonics[i].c7.v,sizeof(float),3,mwad);
    fread(harmonics[i].c8.v,sizeof(float),3,mwad);
  }
  int i = 0;
  for (int x = 1 ; x <= griddims.x;x++)
  {

    for (int y = 1 ; y <= griddims.y;y++)
    {

      for (int z = 1 ; z <= griddims.z;z++)
      {

        fn_vec3  ppl = fn_createVec3((x-(griddims.x*0.5))*gridsize + gridpos.x,(y-(griddims.y*0.5))*gridsize + gridpos.y,(z-(griddims.z*0.5))*gridsize + gridpos.z);

        // if (i % 8000 == 0)
        // {
        //   fn_printVec3(harmonics[i].c0);
        //   fn_printVec3(harmonics[i].c1);
        //   fn_printVec3(harmonics[i].c2);
        //   fn_printVec3(harmonics[i].c3);
        //   fn_printVec3(harmonics[i].c4);
        //   fn_printVec3(harmonics[i].c5);
        //   fn_printVec3(harmonics[i].c6);
        //   fn_printVec3(harmonics[i].c7);
        //   fn_printVec3(harmonics[i].c8);
        // }
        bool hasnan = false;
        if (isnan(harmonics[i].c0.x) || isnan(harmonics[i].c0.y) || isnan(harmonics[i].c0.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c1.x) || isnan(harmonics[i].c1.y) || isnan(harmonics[i].c1.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c2.x) || isnan(harmonics[i].c2.y) || isnan(harmonics[i].c2.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c3.x) || isnan(harmonics[i].c3.y) || isnan(harmonics[i].c3.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c4.x) || isnan(harmonics[i].c4.y) || isnan(harmonics[i].c4.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c5.x) || isnan(harmonics[i].c5.y) || isnan(harmonics[i].c5.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c6.x) || isnan(harmonics[i].c6.y) || isnan(harmonics[i].c6.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c7.x) || isnan(harmonics[i].c7.y) || isnan(harmonics[i].c7.z))
        {
          hasnan = true;
        }
        if (isnan(harmonics[i].c8.x) || isnan(harmonics[i].c8.y) || isnan(harmonics[i].c8.z))
        {
          hasnan = true;
        }
        if (hasnan)
        {
          harmonics[i] = harmonics[i - 1];
        }


        i++;
      }
    }
  }


  fclose(mwad);

  return harmonics;

}

th_Harmonic* th_blankHarmonics(th_Allocator* alloc,int* count,fn_vec3 griddims)
{
  th_Harmonic* harmonics = th_alloc(alloc,sizeof(th_Harmonic)*griddims.x*griddims.y*griddims.z);
  *count = griddims.x*griddims.y*griddims.z;

  for (int i = 0 ; i < griddims.x*griddims.y*griddims.z;i++)
  {
    harmonics[i].c0 = fn_createVec3s(0);
    harmonics[i].c1 = fn_createVec3s(0);
    harmonics[i].c2 = fn_createVec3s(0);
    harmonics[i].c3 = fn_createVec3s(0);
    harmonics[i].c4 = fn_createVec3s(0);
    harmonics[i].c5 = fn_createVec3s(0);
    harmonics[i].c6 = fn_createVec3s(0);
    harmonics[i].c7 = fn_createVec3s(0);
    harmonics[i].c8 = fn_createVec3s(0);

  }

  return harmonics;
}

typedef struct
{
  int start;
  int range;
  th_Harmonic* ret;
  float* weightAccum;
  GLfloat* faces;
  int dimension;
  fn_vec3** cubemapFaceNormals;

  int offset;
  int max_probe_id;
}th_CubemapHarmonicBatchData;

void th_cubeharmonicsbatchthread(void* dat)
{
  th_CubemapHarmonicBatchData* d = (th_CubemapHarmonicBatchData*)dat;

  fn_vec3** cubemapFaceNormals = d->cubemapFaceNormals;
  GLfloat* faces = d->faces;
  int dimension = d->dimension;
  int maximum = d->max_probe_id;
/*
  th_Harmonic ret = *d->ret;
  float weightAccum = *d->weightAccum;*/
  const int ATLAS_WIDTH = 8064;

  for (int c = d->start;c < d->start + d->range;c++)
  {

    int atlasX = c % ATLAS_WIDTH;
    int atlasY = c / ATLAS_WIDTH;

    int faceX = atlasX / 32;
    int faceY = atlasY / 32;
    int faceIdx = faceY * 252 + faceX;

    int probeIdx = faceIdx / 6;
    int faceid = faceIdx % 6;

    if (probeIdx > maximum)
    {
      continue;
    }

    int i = atlasY % 32;//c/(dimension*dimension);
    //GLfloat* face = d->faces[c/(dimension*dimension)];
    int j = atlasX % 32;

    fn_vec3 color = fn_createVec3(faces[c*3 + 0],faces[c*3 + 1],faces[c*3 + 2]);
    float x = (j);
    float y = (i);
    float fU = (2.0 * x / (dimension - 1.0)) - 1.0;
    float fV = (2.0 * y / (dimension - 1.0)) - 1.0;

    fn_vec3 vecx = fn_multVec3s(cubemapFaceNormals[faceid][0],fU);

    fn_vec3 vecy = fn_multVec3s(cubemapFaceNormals[faceid][1],fV);
    fn_vec3 vecz = cubemapFaceNormals[faceid][2];

    fn_vec3 norm = fn_addVec3(vecx,vecy);
    norm = fn_addVec3(norm,vecz);
    norm = fn_normalizeVec3(norm);
    norm = fn_multVec3(norm,fn_createVec3(-1,1,1));

    float weight = texelSolidAngle(x,y,dimension,dimension);

    //weightAccum += weight;

    float basis[9];
    getSHBasis(norm,basis);

    d->ret[probeIdx].c0 = fn_addVec3(d->ret[probeIdx].c0,fn_multVec3s(color,basis[0]*weight));
    d->ret[probeIdx].c1 = fn_addVec3(d->ret[probeIdx].c1,fn_multVec3s(color,basis[1]*weight));
    d->ret[probeIdx].c2 = fn_addVec3(d->ret[probeIdx].c2,fn_multVec3s(color,basis[2]*weight));
    d->ret[probeIdx].c3 = fn_addVec3(d->ret[probeIdx].c3,fn_multVec3s(color,basis[3]*weight));
    d->ret[probeIdx].c4 = fn_addVec3(d->ret[probeIdx].c4,fn_multVec3s(color,basis[4]*weight));
    d->ret[probeIdx].c5 = fn_addVec3(d->ret[probeIdx].c5,fn_multVec3s(color,basis[5]*weight));
    d->ret[probeIdx].c6 = fn_addVec3(d->ret[probeIdx].c6,fn_multVec3s(color,basis[6]*weight));
    d->ret[probeIdx].c7 = fn_addVec3(d->ret[probeIdx].c7,fn_multVec3s(color,basis[7]*weight));
    d->ret[probeIdx].c8 = fn_addVec3(d->ret[probeIdx].c8,fn_multVec3s(color,basis[8]*weight));

    d->weightAccum[probeIdx] = d->weightAccum[probeIdx] + weight;

  }

  // *d->ret = ret;
  // *d->weightAccum = weightAccum;


}



void th_CuebmapToHarmonicsBatch(th_Harmonic* harmonics,int batch_counter,int index_start,GLfloat* faces,int dimension)
{
  // posx
  // negx
  // posy
  // negy
  // posz
  // negz
  static fn_vec3** cubemapFaceNormals = NULL;//[6][3];

  static th_Harmonic** rets = NULL;

  static float** weightAccums = NULL;

  static float* weightAccum = NULL;

  int MAX_PROBES = 252*42;

  if (cubemapFaceNormals == NULL)
  {

    weightAccum = malloc(sizeof(float)*MAX_PROBES);

    rets = malloc(sizeof(th_Harmonic*)*th_getNumThreads());
    weightAccums = malloc(sizeof(float*)*th_getNumThreads());
    for (int i = 0 ; i < th_getNumThreads();i++ )
    {
      rets[i] = malloc(sizeof(th_Harmonic)*MAX_PROBES);
      weightAccums[i] = malloc(sizeof(float)*MAX_PROBES);
    }


    cubemapFaceNormals = malloc(sizeof(fn_vec3*)*6);
    for (size_t i = 0; i < 6; i++) {
      cubemapFaceNormals[i] = malloc(sizeof(fn_vec3)*3);
    }

    cubemapFaceNormals[0][0]=fn_createVec3(0,0,-1);cubemapFaceNormals[0][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[0][2]=fn_createVec3(1,0,0);
    cubemapFaceNormals[1][0]=fn_createVec3(0,0,1);cubemapFaceNormals[1][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[1][2]=fn_createVec3(-1,0,0);
    cubemapFaceNormals[2][0]=fn_createVec3(1,0,0);cubemapFaceNormals[2][1]=fn_createVec3(0,0,1);cubemapFaceNormals[2][2]=fn_createVec3(0,1,0);
    cubemapFaceNormals[3][0]=fn_createVec3(1,0,0);cubemapFaceNormals[3][1]=fn_createVec3(0,0,-1);cubemapFaceNormals[3][2]=fn_createVec3(0,-1,0);
    cubemapFaceNormals[4][0]=fn_createVec3(1,0,0);cubemapFaceNormals[4][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[4][2]=fn_createVec3(0,0,1);
    cubemapFaceNormals[5][0]=fn_createVec3(-1,0,0);cubemapFaceNormals[5][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[5][2]=fn_createVec3(0,0,-1);
  }




  th_Harmonic ret;
  ret.c0 = fn_createVec3s(0);
  ret.c1 = fn_createVec3s(0);
  ret.c2 = fn_createVec3s(0);
  ret.c3 = fn_createVec3s(0);
  ret.c4 = fn_createVec3s(0);
  ret.c5 = fn_createVec3s(0);
  ret.c6 = fn_createVec3s(0);
  ret.c7 = fn_createVec3s(0);
  ret.c8 = fn_createVec3s(0);



  for (int i = 0; i < th_getNumThreads(); i++) {
    // rets[i] = ret;
    // weightAccums[i] = 0;
    for (int j = 0 ; j < MAX_PROBES;j++ )
    {
      rets[i][j] = ret;
      weightAccums[i][j] = 0.0;
    }
  }

  for (int j = 0 ; j < MAX_PROBES;j++ )
  {
    weightAccum[j] = 0.0;
  }

  TH_BEGIN_SCHEDULING((uint)th_getNumThreads(),th_CubemapHarmonicBatchData,8064*8064)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].ret = rets[th_thread_id];
  data[th_thread_id].weightAccum = weightAccums[th_thread_id];
  data[th_thread_id].faces = faces;
  data[th_thread_id].dimension = dimension;
  data[th_thread_id].cubemapFaceNormals = cubemapFaceNormals;
  data[th_thread_id].max_probe_id = batch_counter;
  TH_SCHEDULING_FUNC
  th_setThread(th_cubeharmonicsbatchthread,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING

  for (int i = 0; i < th_getNumThreads(); i++) {
    for (int j = 0 ; j < batch_counter;j++ )
    {
      if (i == 0 )
      {
        harmonics[index_start + j] = ret;
      }

      harmonics[index_start + j].c0 = fn_addVec3(rets[i][j].c0,harmonics[index_start + j].c0);
      harmonics[index_start + j].c1 = fn_addVec3(rets[i][j].c1,harmonics[index_start + j].c1);
      harmonics[index_start + j].c2 = fn_addVec3(rets[i][j].c2,harmonics[index_start + j].c2);
      harmonics[index_start + j].c3 = fn_addVec3(rets[i][j].c3,harmonics[index_start + j].c3);
      harmonics[index_start + j].c4 = fn_addVec3(rets[i][j].c4,harmonics[index_start + j].c4);
      harmonics[index_start + j].c5 = fn_addVec3(rets[i][j].c5,harmonics[index_start + j].c5);
      harmonics[index_start + j].c6 = fn_addVec3(rets[i][j].c6,harmonics[index_start + j].c6);
      harmonics[index_start + j].c7 = fn_addVec3(rets[i][j].c7,harmonics[index_start + j].c7);
      harmonics[index_start + j].c8 = fn_addVec3(rets[i][j].c8,harmonics[index_start + j].c8);


      weightAccum[j] = weightAccum[j] + weightAccums[i][j];
    }
  }


  for (int j = 0 ; j < batch_counter;j++ )
  {
    harmonics[index_start + j].c0 = fn_multVec3s(harmonics[index_start + j].c0,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c1 = fn_multVec3s(harmonics[index_start + j].c1,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c2 = fn_multVec3s(harmonics[index_start + j].c2,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c3 = fn_multVec3s(harmonics[index_start + j].c3,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c4 = fn_multVec3s(harmonics[index_start + j].c4,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c5 = fn_multVec3s(harmonics[index_start + j].c5,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c6 = fn_multVec3s(harmonics[index_start + j].c6,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c7 = fn_multVec3s(harmonics[index_start + j].c7,4.f * PI / weightAccum[j]);
    harmonics[index_start + j].c8 = fn_multVec3s(harmonics[index_start + j].c8,4.f * PI / weightAccum[j]);

    harmonics[index_start + j].c0 = fn_multVec3s(harmonics[index_start + j].c0,PI*(1.0/1.0));
    harmonics[index_start + j].c1 = fn_multVec3s(harmonics[index_start + j].c1,PI*(2.0/3.0));
    harmonics[index_start + j].c2 = fn_multVec3s(harmonics[index_start + j].c2,PI*(2.0/3.0));
    harmonics[index_start + j].c3 = fn_multVec3s(harmonics[index_start + j].c3,PI*(2.0/3.0));
    harmonics[index_start + j].c4 = fn_multVec3s(harmonics[index_start + j].c4,PI*(1.0/4.0));
    harmonics[index_start + j].c5 = fn_multVec3s(harmonics[index_start + j].c5,PI*(1.0/4.0));
    harmonics[index_start + j].c6 = fn_multVec3s(harmonics[index_start + j].c6,PI*(1.0/4.0));
    harmonics[index_start + j].c7 = fn_multVec3s(harmonics[index_start + j].c7,PI*(1.0/4.0));
    harmonics[index_start + j].c8 = fn_multVec3s(harmonics[index_start + j].c8,PI*(1.0/4.0));
  }




  // //convoltuiion
  // ret.c0 = fn_multVec3s(ret.c0,PI*(1.0));
  // ret.c1 = fn_multVec3s(ret.c1,PI*(2.0/3.0));
  // ret.c2 = fn_multVec3s(ret.c2,PI*(2.0/3.0));
  // ret.c3 = fn_multVec3s(ret.c3,PI*(2.0/3.0));
  // ret.c4 = fn_multVec3s(ret.c4,PI*(1.0/4.0));
  // ret.c5 = fn_multVec3s(ret.c5,PI*(1.0/4.0));
  // ret.c6 = fn_multVec3s(ret.c6,PI*(1.0/4.0));
  // ret.c7 = fn_multVec3s(ret.c7,PI*(1.0/4.0));
  // ret.c8 = fn_multVec3s(ret.c8,PI*(1.0/4.0));

  // printf("%f\n", 4.f * PI / weightAccum);
//  return ret;
}
