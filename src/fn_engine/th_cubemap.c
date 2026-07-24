#include "th_cubemap.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../fn_window.h"
#include "th_gpu.h"
#include "../th_fopen.h"

GLuint th_createCubemap(GLfloat** faces,int dimension)
{
unsigned int textureID;
glGenTextures(1, &textureID);
glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);


for( int i = 0; i < 6; i++)
{

    glTexImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
        0, GL_RGBA16F, dimension, dimension, 0, GL_RGBA, GL_FLOAT, faces[i]
    );

}

glBindTexture( GL_TEXTURE_CUBE_MAP, 0);
return textureID;
}

void th_saveCubemap(GLfloat** faces,int dimension,const char* filename)
{
  FILE* fp = th_fopen(filename,"w");
  for( int i = 0; i < 6; i++)
  {
    for (int j = 0 ; j < dimension*dimension;j++)
    {
      fprintf(fp, "%.4f %.4f %.4f %.4f ",faces[i][j*4 + 0],faces[i][j*4 + 1],faces[i][j*4 + 2],faces[i][j*4 + 3] );
    }

  }
  fclose(fp);
}

GLfloat** th_loadCubemap(int dimension,const char* filename)
{
  GLfloat** out = malloc(sizeof(GLfloat*)*6);

  FILE* fp = th_fopen(filename,"r");
  for( int i = 0; i < 6; i++)
  {
    out[i] = malloc(sizeof(GLfloat)*dimension*dimension*3);
    for (int j = 0 ; j < dimension*dimension;j++)
    {
      fscanf(fp, "%f %f %f ",&out[i][j*3 + 0],&out[i][j*3 + 1],&out[i][j*3 + 2] );
    }

  }
  fclose(fp);
  return out;
}



void th_loadCubemapWAD(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount,const char* wadlocation)
{
  int layers = cubecount;

  GLfloat* wad_data;
  GLfloat** color_data;
  GLfloat* minmaxdata;

  minmaxdata = malloc(sizeof(GLfloat)*6*layers);
  wad_data = malloc(sizeof(GLfloat)*6*resolution*resolution*layers);
  color_data = malloc(sizeof(GLfloat*)*mips);
  for (int i = 0 ; i < mips;i++)
  {
    unsigned int mipWidth = resolution * powf(0.5, i);
    unsigned int mipHeight = resolution * powf(0.5, i);
    color_data[i] = malloc(sizeof(GLfloat)*6*mipWidth*mipHeight*layers*3);
  }


  char filename[128];
  sprintf(filename,"%s%i.cwad",wadlocation,0);
  FILE* mwad = th_fopen(filename,"rb");
  if (mwad == NULL)
  {
    printf("%s\n","CANNOT OPEN" );
  }
  fread(minmaxdata,sizeof(GLfloat),6*layers,mwad);
  fread(wad_data,sizeof(GLfloat),6*resolution*resolution*layers,mwad);
  for (int i = 0 ; i < mips;i++)
  {
    unsigned int mipWidth = resolution * powf(0.5, i);
    unsigned int mipHeight = resolution * powf(0.5, i);
    fread(color_data[i],sizeof(GLfloat),6*mipWidth*mipHeight*layers*3,mwad);
    for (unsigned int f = 0; f < 6*mipWidth*mipHeight*layers*3; f++) {
      if (isnan(color_data[i][f]))
      {
        color_data[i][f] = 0.0;
      }
    }
  }
  fclose(mwad);

  // mips = 1;
  int depth_mips = 6;

  glGenTextures(1, depthtex);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *depthtex);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, depth_mips - 1);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  // glTexImage3D(
  //   GL_TEXTURE_CUBE_MAP_ARRAY,
  //   0, GL_R16F, resolution, resolution,layers*6, 0, GL_RED, GL_FLOAT, NULL
  // );
  glTexStorage3D(
    GL_TEXTURE_CUBE_MAP_ARRAY,
    depth_mips, GL_R16F, resolution, resolution,layers*6);

  glGenTextures(1, colortex);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glTexStorage3D(
    GL_TEXTURE_CUBE_MAP_ARRAY,
    mips, GL_RGB16F, resolution, resolution,layers*6);



  // FILE* fp = th_fopen(filename,"r");
  for (int l = 0 ; l < cubecount;l++)
  {
    if (min != NULL && max != NULL)
    {
      min[l] = fn_createVec3(minmaxdata[l*6 + 0],minmaxdata[l*6 + 1],minmaxdata[l*6 + 2]);
      max[l] = fn_createVec3(minmaxdata[l*6 + 3],minmaxdata[l*6 + 4],minmaxdata[l*6 + 5]);
    }


    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *depthtex);

    GLfloat** mip_data = malloc(sizeof(GLfloat*)*depth_mips);
    for (int i = 0; i < depth_mips; i++) {
      unsigned int mipWidth = resolution * powf(0.5, i);
      unsigned int mipHeight = resolution * powf(0.5, i);
      mip_data[i] = malloc(sizeof(GLfloat)*6*mipWidth*mipHeight);
      if (i == 0)
      {
        memcpy(&mip_data[0][0],&wad_data[l*6*resolution*resolution],6*resolution*resolution*sizeof(GLfloat));
        // for (int face = 0; face < 6; face++) {
        //   for (int mip_x = 0; mip_x < mipWidth; mip_x++) {
        //     for (int mip_y = 0; mip_y < mipHeight; mip_y++) {
        //
        //        mip_data[i][mip_x + mip_y*mipWidth + face*mipWidth*mipHeight] = 0;
        //     }
        //   }
        // }
      }
      else
      {
        for (int face = 0; face < 6; face++) {
          unsigned int mipWidth_old = resolution * powf(0.5, i-1);
          unsigned int mipHeight_old = resolution * powf(0.5, i-1);

          for (unsigned int mip_x = 0; mip_x < mipWidth; mip_x++) {
            for (unsigned int mip_y = 0; mip_y < mipHeight; mip_y++) {
              // printf("%i %i\n", mip_x,mip_y);
              // printf("%i\n",mip_x*2 + mip_y*2*mipWidth_old + face*mipWidth_old*mipHeight_old );
              GLfloat a = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + face*mipWidth_old*mipHeight_old];
              GLfloat b = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + 1 + face*mipWidth_old*mipHeight_old];
              GLfloat c = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + mipWidth_old + face*mipWidth_old*mipHeight_old];
              GLfloat d = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + 1 + mipWidth_old + face*mipWidth_old*mipHeight_old];

              GLfloat mval = fmin(a,fmin(b,fmin(c,d)));
              mip_data[i][mip_x + mip_y*mipWidth + face*mipWidth*mipHeight] = mval;
            }
          }
        }

      }
    }

    for (int k = 0 ; k <depth_mips;k++)
    {
      unsigned int mipWidth = resolution * powf(0.5, k);
      unsigned int mipHeight = resolution * powf(0.5, k);
      //  printf("%i %i\n",mipWidth,mipHeight );
      for( int i = 0; i < 6; i++)
      {
        // if (k ==0 )
        // {
        //   glTexSubImage3D(
        //     GL_TEXTURE_CUBE_MAP_ARRAY,
        //     0,0,0,l*6 + i, resolution, resolution,1, GL_RED, GL_FLOAT, &wad_data[l*6*resolution*resolution + i*resolution*resolution]
        //   );
        // }
        // else
        // {
        glTexSubImage3D(
          GL_TEXTURE_CUBE_MAP_ARRAY,
          k,0,0,l*6 + i, mipWidth, mipHeight,1, GL_RED, GL_FLOAT, &mip_data[k][i*mipWidth*mipHeight]
        );
        // &wad_data[l*6*resolution*resolution + i*resolution*resolution]
        //  }



      }
    }


    for (int i = 0; i < depth_mips; i++) {
      free(mip_data[i]);
    }
    free(mip_data);




    glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
    // for (int k = 0 ; k < layers;k++)
    // {
    for (int i = 0 ; i <mips;i++)
    {
      unsigned int mipWidth = resolution * powf(0.5, i);
      unsigned int mipHeight = resolution * powf(0.5, i);
      for( int j = 0; j < 6; j++)
      {

        glTexSubImage3D(
          GL_TEXTURE_CUBE_MAP_ARRAY,
          i,0,0,l*6 + j, mipWidth, mipHeight,1, GL_RGB, GL_FLOAT, &color_data[i][l*6*mipWidth*mipHeight*3 + j*mipWidth*mipHeight*3 ]
        );



      }
    }

    glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0);
  }

  fn_getGLError();
  free(minmaxdata);
  free(wad_data);

  for (int i = 0 ; i < mips;i++)
  {

    free(color_data[i]);
  }
  free(color_data);

}

typedef struct
{
  uint8_t identifier[12];
  uint32_t endianness;
  uint32_t glType;
  uint32_t glTypeSize;
  uint32_t glFormat;
  uint32_t glInternalFormat;
  uint32_t glBaseInternalFormat;
  uint32_t pixelWidth;
  uint32_t pixelHeight;
  uint32_t pixelDepth;
  uint32_t numberOfArrayElements;
  uint32_t numberOfFaces;
  uint32_t numberOfMipmapLevels;
  uint32_t bytesOfKeyValueData;
}KTX_header;

static bool init_vram_cubemaps = false;
static GLuint cached_depth_tex = 0;
static GLuint cached_color_tex = 0;

void th_loadCubemapCompressed(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount,const char* wadlocation)
{
  int layers = cubecount;

  GLfloat* wad_data;
  // GLfloat** color_data;
  GLfloat* minmaxdata;
  //
  minmaxdata = malloc(sizeof(GLfloat)*6*layers);
  wad_data = malloc(sizeof(GLfloat)*6*resolution*resolution*layers);
  // color_data = malloc(sizeof(GLfloat*)*mips);
  // for (int i = 0 ; i < mips;i++)
  // {
  //   unsigned int mipWidth = resolution * powf(0.5, i);
  //   unsigned int mipHeight = resolution * powf(0.5, i);
  //   color_data[i] = malloc(sizeof(GLfloat)*6*mipWidth*mipHeight*layers*3);
  // }


  char filename[128];
  sprintf(filename,"%sdistance.bin",wadlocation);
  FILE* mwad = th_fopen(filename,"rb");
  if (mwad == NULL)
  {
    printf("%s\n","CANNOT OPEN" );
  }
  fread(minmaxdata,sizeof(GLfloat),6*layers,mwad);
  fread(wad_data,sizeof(GLfloat),6*resolution*resolution*layers,mwad);


  fclose(mwad);

  // mips = 1;
  int depth_mips = 6;

  if (!init_vram_cubemaps)
  {
    glGenTextures(1, depthtex);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *depthtex);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, depth_mips - 1);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glTexStorage3D(
      GL_TEXTURE_CUBE_MAP_ARRAY,
      depth_mips, GL_R16F, resolution, resolution,TH_MAX_CUBEMAPS*6);

    // glGenTextures(1, colortex);
    // glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    //
    // glTexStorage3D(
    //   GL_TEXTURE_CUBE_MAP_ARRAY,
    //   mips, GL_RGB16F, resolution, resolution,layers*6);
  }
  else
  {
    *depthtex = cached_depth_tex;
    //*colortex = cached_color_tex;
  }




  // FILE* fp = th_fopen(filename,"r");
  for (int l = 0 ; l < cubecount;l++)
  {
    if (min != NULL && max != NULL)
    {
      min[l] = fn_createVec3(minmaxdata[l*6 + 0],minmaxdata[l*6 + 1],minmaxdata[l*6 + 2]);
      max[l] = fn_createVec3(minmaxdata[l*6 + 3],minmaxdata[l*6 + 4],minmaxdata[l*6 + 5]);
    }


    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *depthtex);

    GLfloat** mip_data = malloc(sizeof(GLfloat*)*depth_mips);
    for (int i = 0; i < depth_mips; i++) {
      unsigned int mipWidth = resolution * powf(0.5, i);
      unsigned int mipHeight = resolution * powf(0.5, i);
      mip_data[i] = malloc(sizeof(GLfloat)*6*mipWidth*mipHeight);
      if (i == 0)
      {
        memcpy(&mip_data[0][0],&wad_data[l*6*resolution*resolution],6*resolution*resolution*sizeof(GLfloat));
      }
      else
      {
        for (int face = 0; face < 6; face++) {
          unsigned int mipWidth_old = resolution * powf(0.5, i-1);
          unsigned int mipHeight_old = resolution * powf(0.5, i-1);

          for (unsigned int mip_x = 0; mip_x < mipWidth; mip_x++) {
            for (unsigned int mip_y = 0; mip_y < mipHeight; mip_y++) {
              // printf("%i %i\n", mip_x,mip_y);
              // printf("%i\n",mip_x*2 + mip_y*2*mipWidth_old + face*mipWidth_old*mipHeight_old );
              GLfloat a = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + face*mipWidth_old*mipHeight_old];
              GLfloat b = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + 1 + face*mipWidth_old*mipHeight_old];
              GLfloat c = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + mipWidth_old + face*mipWidth_old*mipHeight_old];
              GLfloat d = mip_data[i - 1][mip_x*2 + mip_y*2*mipWidth_old + 1 + mipWidth_old + face*mipWidth_old*mipHeight_old];

              GLfloat mval = fmin(a,fmin(b,fmin(c,d)));
              mip_data[i][mip_x + mip_y*mipWidth + face*mipWidth*mipHeight] = mval;
            }
          }
        }

      }
    }

    for (int k = 0 ; k <depth_mips;k++)
    {
      unsigned int mipWidth = resolution * powf(0.5, k);
      unsigned int mipHeight = resolution * powf(0.5, k);
      //  printf("%i %i\n",mipWidth,mipHeight );
      for( int i = 0; i < 6; i++)
      {
        glTexSubImage3D(
          GL_TEXTURE_CUBE_MAP_ARRAY,
          k,0,0,l*6 + i, mipWidth, mipHeight,1, GL_RED, GL_FLOAT, &mip_data[k][i*mipWidth*mipHeight]
        );

      }
    }

    for (int i = 0; i < depth_mips; i++) {
      free(mip_data[i]);
    }
    free(mip_data);
  }

  free(wad_data);
  free(minmaxdata);

  //load ktx

  char filename_ktx[128];
  sprintf(filename_ktx,"%scubes.ktx",wadlocation);

  FILE* f = th_fopen(filename_ktx, "rb");

  KTX_header header;
  fread(&header, sizeof(KTX_header), 1, f);

  fseek(f, header.bytesOfKeyValueData, SEEK_CUR);

  if (!init_vram_cubemaps)
  {

  glGenTextures(1, colortex);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glTexStorage3D(
    GL_TEXTURE_CUBE_MAP_ARRAY,
    mips, header.glInternalFormat, resolution, resolution,TH_MAX_CUBEMAPS*6);

  }
  else
  {
    *colortex = cached_color_tex;
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
  }

  //printf("KTX DEBUG %i\n", header.numberOfMipmapLevels);

  for (uint32_t i = 0 ; i < header.numberOfMipmapLevels;i++)
  {
    uint32_t imageSize;
    fread(&imageSize, 4, 1, f);

    imageSize = (imageSize / (header.numberOfArrayElements*header.numberOfFaces));
    //printf("KTX DEBUG ISIZE %i\n", imageSize);

    for (uint32_t j = 0 ; j < header.numberOfArrayElements;j++)
    {

      for (uint32_t k = 0 ; k < header.numberOfFaces;k++)
      {

        // tex->data_sizes[i] = imageSize;
        uint8_t* data = malloc(sizeof(uint8_t)*imageSize);
        fread(data, 1, imageSize, f);
        //
        // tex->data[i] = data;

        uint32_t width  = fmax(1, resolution >> i);
        uint32_t height = fmax(1, resolution >> i);

        //printf("KTX DEBUG IM %i %i %i\n", i,j,k);
        glCompressedTexSubImage3D(
          GL_TEXTURE_CUBE_MAP_ARRAY,
          i,                          // mip level
          0, 0, j*6 + k,                   // offsets
          width, height, 1,
          header.glInternalFormat,             // same format
          imageSize,        // compressed size
          data               // compressed data
        );

        free(data);
      }
    }

    uint32_t padding = (4 - (imageSize % 4)) % 4;
    if (padding > 0) {
      fseek(f, padding, SEEK_CUR);
    }
  }

  fclose(f);

  cached_depth_tex = *depthtex;
  cached_color_tex = *colortex;
  init_vram_cubemaps = true;





}

void th_blankCubemap(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount)
{
  for (int i = 0;i < cubecount;i++)
  {
    if (min != NULL && max != NULL)
    {
      min[i] = fn_createVec3(0,0,0);
      max[i] = fn_createVec3(0,0,0);
    }

  }

  GLfloat** depthData = malloc(sizeof(GLfloat*)*6);
  for( int i = 0; i < 6; i++)
  {
    depthData[i] = malloc(sizeof(GLfloat)*resolution*resolution);
    memset(depthData[i],0,sizeof(GLfloat)*resolution*resolution);
    // for (int j = 0 ; j < resolution*resolution;j++)
    // {
    //   fscanf(fp, "%f ",&depthData[i][j] );
    // }

  }

  glGenTextures(1, depthtex);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *depthtex);

  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  int layers =1;
  glTexImage3D(
      GL_TEXTURE_CUBE_MAP_ARRAY,
      0, GL_R16F, resolution, resolution,layers*6, 0, GL_RED, GL_FLOAT, NULL
  );

  for (int j = 0 ; j < layers;j++)
  {
    for( int i = 0; i < 6; i++)
    {

        // glTexImage2D(
        //     GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
        //     0, GL_R16F, resolution, resolution, 0, GL_RED, GL_FLOAT, depthData[i]
        // );
        glTexSubImage3D(
            GL_TEXTURE_CUBE_MAP_ARRAY,
            0,0,0,j*6 + i, resolution, resolution,1, GL_RED, GL_FLOAT, depthData[i]
        );


    }
  }

  glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0);


  //output cube
  GLfloat*** filterData = malloc(sizeof(GLfloat**)*mips);
  for (int i = 0 ; i <mips;i++)
  {

    unsigned int mipWidth = resolution * powf(0.5, i);
    unsigned int mipHeight = resolution * powf(0.5, i);
    filterData[i] = malloc(sizeof(GLfloat*)*6);
    for (int j = 0 ; j < 6;j++)
    {
      filterData[i][j] = malloc(sizeof(GLfloat)*mipWidth*mipHeight*3);
      memset(filterData[i][j],0,sizeof(GLfloat)*mipWidth*mipHeight*3);
      // for (int k = 0 ; k < mipWidth*mipHeight;k++)
      // {
      //   fscanf(fp, "%f %f %f %f ",&filterData[i][j][k*4 + 0],&filterData[i][j][k*4 + 1],&filterData[i][j][k*4 + 2],&filterData[i][j][k*4 + 3] );
      // }
    }
  }

  glGenTextures(1, colortex);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, *colortex);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);



  glTexStorage3D(
      GL_TEXTURE_CUBE_MAP_ARRAY,
      mips, GL_RGB16F, resolution, resolution,layers*6);

  for (int k = 0 ; k < layers;k++)
  {
    for (int i = 0 ; i <mips;i++)
    {
      unsigned int mipWidth = resolution * powf(0.5, i);
      unsigned int mipHeight = resolution * powf(0.5, i);
    for( int j = 0; j < 6; j++)
    {

        // glTexImage2D(
        //     GL_TEXTURE_CUBE_MAP_POSITIVE_X + j,
        //     i, GL_RGBA16F, mipWidth, mipHeight, 0, GL_RGBA, GL_FLOAT, filterData[i][j]
        // );
        glTexSubImage3D(
            GL_TEXTURE_CUBE_MAP_ARRAY,
            i,0,0,k*6 + j, mipWidth, mipHeight,1, GL_RGB, GL_FLOAT, filterData[i][j]
        );



      }

    }
  }

  for (int i = 0 ; i <mips;i++)
  {

    for (int j = 0 ; j < 6;j++)
    {
      free(filterData[i][j]);

    }
    free(filterData[i]);
  }
  free(filterData);

  glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0);



  for( int i = 0; i < 6; i++)
  {
    free(depthData[i]);
  }
  free(depthData);

}

static fn_mat4 r_camera(fn_vec3 pos,fn_vec2 angles)
{

  angles.y = fn_clamp(angles.y,-fn_radians(90),fn_radians(90));
  fn_mat4 modelView = fn_identityMat4();//glm::lookAt(-getLocation(),getLook(),glm::vec3(0,-1,0));
  //modelView = glm::lookAt(getLocation(),getLook(),glm::vec3(0,1,0));
  //  modelView = fn_rotate(modelView, -glm::radians(camRoll), glm::vec3(0.0f, 0.0f, 1.0f));
  modelView = fn_rotate(modelView, angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));
  modelView = fn_rotate(modelView, angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
//  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  modelView = fn_translate(modelView,fn_multVec3(pos,fn_createVec3(1,1,1)));//*glm::vec3(-1,1,-1));
  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  return modelView;
}

static bool init_skycubemap_vram = false;
static GLuint prefilterMap_cache = 0;
static GLuint skytex_cache = 0;
static th_ArrayObject post;
static th_FrameBuffer frame;

void th_createSkyCubemap(int resolution,r_Shader* shader,GLuint* skytex,fn_vec3 sunpos,r_Shader* prefiltershader,fn_vec3 atm_rayleigh,float sun_intensity,fn_vec3 sun_color,bool send_cloud_info,\
float cloud_enable,\
float horizon_height)\
{
  fn_mat4 orthomat = fn_ortho(0,resolution,0,resolution);

  if (!init_skycubemap_vram)
  {
      th_createFramebufferPrefilter(&frame,resolution,resolution);
  }

  fn_mat4 mats[6];
  mats[1] = r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(-90),fn_radians(0)));// negx
  mats[0] = r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(90),fn_radians(0)));// posx
  mats[4] = r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(0),fn_radians(0))); //posz
  mats[5] =  r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(180),fn_radians(0)));//negz
  mats[3] = r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(0),fn_radians(90)));//negy
  mats[2] = r_camera(fn_createVec3s(0),fn_createVec2(fn_radians(0),fn_radians(-90)));//posy
  th_GpuData screen_data;

  screen_data.verts = malloc(sizeof(th_Vertex)*3);
  screen_data.indices = malloc(sizeof(GLuint)*3);
  screen_data.vertcount = 3;
  screen_data.indicecount = 3;
  screen_data.indices[0] = 0;
  screen_data.indices[1] = 1;
  screen_data.indices[2] = 2;
  // post_data.indices[3] = 1;
  // post_data.indices[4] = 2;
  // post_data.indices[5] = 3;
  //post_data.verts[0].position = fn_createVec3(s_w*2,s_h*2,0);
  screen_data.verts[0].position = fn_createVec3(resolution*2,0,0);
  screen_data.verts[1].position = fn_createVec3(0,0,0);
  screen_data.verts[2].position = fn_createVec3(0,resolution*2,0);
  //  post_data.verts[0].texCoord = fn_createVec2(1*2,0);
  screen_data.verts[0].texCoord = fn_createVec2(1*2,0);
  screen_data.verts[1].texCoord = fn_createVec2(0,0);
  screen_data.verts[2].texCoord = fn_createVec2(0,1*2);
  //post_data.verts[0].normal = fn_createVec3(1,1,1);
  screen_data.verts[0].normal = fn_createVec3(1,1,1);
  screen_data.verts[1].normal = fn_createVec3(1,1,1);
  screen_data.verts[2].normal = fn_createVec3(1,1,1);
  //post_data.verts[0].tangent = fn_createVec3(1,1,1);
  screen_data.verts[0].tangent = fn_createVec3(1,1,1);
  screen_data.verts[1].tangent = fn_createVec3(1,1,1);
  screen_data.verts[2].tangent = fn_createVec3(1,1,1);
  //post_data.verts[0].bitangent = fn_createVec3(1,1,1);
  screen_data.verts[0].bitangent = fn_createVec3(1,1,1);
  screen_data.verts[1].bitangent = fn_createVec3(1,1,1);
  screen_data.verts[2].bitangent = fn_createVec3(1,1,1);

  if (!init_skycubemap_vram)
  {
    th_createVbo(&post,screen_data,TH_NOINSTANCE | TH_NOCOMMAND | TH_NONORMALTANGENT);
  }
  // screen_data.verts = malloc(sizeof(th_Vertex)*4);
  // screen_data.indices = malloc(sizeof(GLuint)*6);
  // screen_data.vertcount = 4;
  // screen_data.indicecount = 6;
  // screen_data.indices[0] = 0;
  // screen_data.indices[1] = 1;
  // screen_data.indices[2] = 3;
  // screen_data.indices[3] = 1;
  // screen_data.indices[4] = 2;
  // screen_data.indices[5] = 3;
  // screen_data.verts[0].position = fn_createVec3(resolution,resolution,0);
  // screen_data.verts[1].position = fn_createVec3(resolution,0,0);
  // screen_data.verts[2].position = fn_createVec3(0,0,0);
  // screen_data.verts[3].position = fn_createVec3(0,resolution,0);
  // screen_data.verts[0].texCoord = fn_createVec2(1,0);
  // screen_data.verts[1].texCoord = fn_createVec2(1,1);
  // screen_data.verts[2].texCoord = fn_createVec2(0,1);
  // screen_data.verts[3].texCoord = fn_createVec2(0,0);
  // screen_data.verts[0].normal = fn_createVec3(1,1,1);
  // screen_data.verts[1].normal = fn_createVec3(1,1,1);
  // screen_data.verts[2].normal = fn_createVec3(1,1,1);
  // screen_data.verts[3].normal = fn_createVec3(1,1,1);
  // screen_data.verts[0].tangent = fn_createVec3(1,1,1);
  // screen_data.verts[1].tangent = fn_createVec3(1,1,1);
  // screen_data.verts[2].tangent = fn_createVec3(1,1,1);
  // screen_data.verts[3].tangent = fn_createVec3(1,1,1);
  // screen_data.verts[0].bitangent = fn_createVec3(1,1,1);
  // screen_data.verts[1].bitangent = fn_createVec3(1,1,1);
  // screen_data.verts[2].bitangent = fn_createVec3(1,1,1);
  // screen_data.verts[3].bitangent = fn_createVec3(1,1,1);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  //th_createVbo(&post,screen_data,TH_NOINSTANCE | TH_NOCOMMAND | TH_NONORMALTANGENT);

  if (!init_skycubemap_vram)
  {
    glGenTextures(1, skytex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *skytex);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 4);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);


    for( int i = 0; i < 6; i++)
    {

      glTexImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
        0, GL_RGBA16F, resolution, resolution, 0, GL_RGBA, GL_FLOAT, NULL
      );


    }

    skytex_cache = *skytex;
  }
  else
  {
    *skytex = skytex_cache;
    glBindTexture(GL_TEXTURE_CUBE_MAP, *skytex);
  }

  // glGenerateMipmap(GL_TEXTURE_CUBE_MAP);



  glBindTexture( GL_TEXTURE_CUBE_MAP, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
  glViewport(0,0,resolution,resolution);

  r_bindShader(shader);
  r_send3f(shader,sunpos,"uSunPos");
  r_sendmat4(shader,orthomat,"modelViewprojection");
  r_send3f(shader,atm_rayleigh,"rayleigh");
  r_send3f(shader,sun_color,"sun_color");
  r_sendf(shader,sun_intensity,"sun_intensity");

  if (send_cloud_info)
  {
    r_sendf(shader,cloud_enable,"cloud_enable");
    r_sendf(shader,horizon_height,"horizon_height");
  }




  for (int j = 0; j < 6;j++)
  {
    r_sendmat4(shader,mats[j],"viewMat");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, *skytex, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    th_renderArray(&post,6,0,0,0);
  }


  GLuint prefilterMap;
  if (!init_skycubemap_vram)
  {

  glGenTextures(1, &prefilterMap);
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 4);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glTexStorage2D(
    GL_TEXTURE_CUBE_MAP,
    5, GL_RGB16F, resolution, resolution);

   prefilterMap_cache = prefilterMap;

  }
  else
  {

    prefilterMap = prefilterMap_cache;
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

  }
  // for( int i = 0; i < 6; i++)
  // {
  //
  //     glTexImage2D(
  //         GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
  //         0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, NULL
  //     );
  //
  //
  // }
  //  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, *skytex);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
  // glActiveTexture(GL_TEXTURE1);
  // glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

  r_bindShader(prefiltershader);
  r_sendTextureUniform(prefiltershader,0,"environmentMap");
  r_sendf(prefiltershader,resolution,"resolution");

  fn_vec3 cubemapFaceNormals[6][3];
  cubemapFaceNormals[0][0]=fn_createVec3(0,0,-1);cubemapFaceNormals[0][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[0][2]=fn_createVec3(1,0,0);
  cubemapFaceNormals[1][0]=fn_createVec3(0,0,1);cubemapFaceNormals[1][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[1][2]=fn_createVec3(-1,0,0);
  cubemapFaceNormals[2][0]=fn_createVec3(1,0,0);cubemapFaceNormals[2][1]=fn_createVec3(0,0,1);cubemapFaceNormals[2][2]=fn_createVec3(0,1,0);
  cubemapFaceNormals[3][0]=fn_createVec3(1,0,0);cubemapFaceNormals[3][1]=fn_createVec3(0,0,-1);cubemapFaceNormals[3][2]=fn_createVec3(0,-1,0);
  cubemapFaceNormals[4][0]=fn_createVec3(1,0,0);cubemapFaceNormals[4][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[4][2]=fn_createVec3(0,0,1);
  cubemapFaceNormals[5][0]=fn_createVec3(-1,0,0);cubemapFaceNormals[5][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[5][2]=fn_createVec3(0,0,-1);

  const int miplevels = 5;
  for (int i = 0 ; i < miplevels;i++)
  {
    unsigned int mipWidth = resolution * powf(0.5, i);
    unsigned int mipHeight = resolution * powf(0.5, i);
    glViewport(0, 0, mipWidth, mipHeight);
    float roughness = (float)i / (float)(miplevels - 1);
    r_sendf(prefiltershader,roughness,"roughness");
    fn_mat4 postmatrix = fn_ortho(0,mipWidth,0,mipHeight);
    r_sendmat4(prefiltershader,postmatrix,"modelViewprojection");
    r_send2f(prefiltershader,fn_createVec2(1.0/mipWidth,1.0/mipHeight),"invResolution");
    for (int j = 0; j < 6;j++)
    {
      r_send3f(prefiltershader,cubemapFaceNormals[j][0],"Normcomp0");
      r_send3f(prefiltershader,cubemapFaceNormals[j][1],"Normcomp1");
      r_send3f(prefiltershader,cubemapFaceNormals[j][2],"Normcomp2");
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, prefilterMap, i);
      glClear(GL_COLOR_BUFFER_BIT);
      th_renderArray(&post,6,0,0,0);

    }
  }

  //glDeleteTextures(1,skytex);


  *skytex = prefilterMap;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  // th_freeFramebuffer(&frame);
  // th_freeVbo(&post);
  free(screen_data.verts);
  free(screen_data.indices);
  //free(frame.textures);


  init_skycubemap_vram = true;


}
