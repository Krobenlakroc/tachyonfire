#include "r_texture.h"
#include "../spng.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fn_profile.h"
#include "../fn_window.h"
#include "th_threads.h"
#include "../th_fopen.h"

static int texIndex = 0;

static int* pvars_alloced = NULL;



static GLuint boundtexttures[128][4];

static bool setbound = false;

static int cur_unit = 8;//first 6 are reserved UP TO GL_TEXTURE5



static int enumtoid(GLenum tmode)
{
  if (tmode == GL_TEXTURE_2D)
  {
    return 0;
  }
  if (tmode == GL_TEXTURE_2D_ARRAY)
  {
    return 1;
  }
  return 2;
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


typedef struct
{
  KTX_header header;
  int width;
  int height;
  bool alpha;
  bool mono;
  GLubyte** data;//one for each mip
  uint32_t* data_sizes;
}KTX_image;

void th_loadKTX(const char* filename, KTX_image* tex)
{
  FILE* f = th_fopen(filename, "rb");
  if (f == NULL)
  {
    printf("Could not open KTX file %s\n",filename);
  }

  KTX_header header;
  fread(&header, sizeof(KTX_header), 1, f);

  tex->header = header;

  fseek(f, header.bytesOfKeyValueData, SEEK_CUR);

  tex->data = malloc(sizeof(GLubyte*)*header.numberOfMipmapLevels);
  tex->data_sizes = malloc(sizeof(uint32_t)*header.numberOfMipmapLevels);

  for (uint32_t i = 0 ; i < header.numberOfMipmapLevels;i++)
  {
    uint32_t imageSize;
    fread(&imageSize, 4, 1, f);

    tex->data_sizes[i] = imageSize;
    uint8_t* data = malloc(sizeof(uint8_t)*imageSize);
    fread(data, 1, imageSize, f);

    tex->data[i] = data;
  }
  fclose(f);
}

int r_getTextureUnit()
{
  cur_unit++;
  if (cur_unit == 33)
  {
    cur_unit = 9;
  }
  return cur_unit - 1;
}
void r_resetTextureSubSystem()
{
  cur_unit = 8;
  int i;
  for (i = 0 ; i < texIndex;i++)
  {
    fn_deleteProfileVarID(pvars_alloced[i]);
  }
  free(pvars_alloced);
  pvars_alloced = NULL;

  fn_freeDeletedProfileMem();
  texIndex = 0;

  setbound = false;
}

void loadPngImage(const char *name, int* outWidth, int* outHeight, bool* outHasAlpha, GLubyte **outData,bool* outMono,size_t* output_size)
{
  FILE *fp = NULL;

  if ((fp = th_fopen(name, "rb")) == NULL)
    printf("%s %s\n","PNG error 1",name );

  /* Create a context */
  spng_ctx *ctx = spng_ctx_new(0);

  spng_set_png_file(ctx, fp);
  size_t out_size;
  /* Calculate output image size */
  spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);
  if (output_size != NULL)
  {
    *output_size = out_size;
  }

  *outData = (unsigned char*) malloc(out_size);

  struct spng_ihdr ihdr;
  spng_get_ihdr(ctx, &ihdr);
  *outWidth = ihdr.width;
  *outHeight = ihdr.height;
  // printf("%i %i %i %i %s\n",*outWidth,*outHeight,ihdr.bit_depth,ihdr.color_type,name);

  bool convert_manual = false;
  if (ihdr.bit_depth == 16 && ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
  {
    convert_manual = true;
  }

  *outHasAlpha = false;
  *outMono = false;
  if (ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
    *outHasAlpha = true;
  if (ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
  {
    *outMono = true;
  }

  /* Get an 8-bit RGBA image regardless of PNG format */
  //spng_decode_image(ctx, SPNG_FMT_RGBA8, *outData, out_size, 0);
  int fmt = SPNG_FMT_RGB8;
  if((*outHasAlpha))
  {
    fmt =SPNG_FMT_RGBA8;
  }
  if (*outMono)
  {
    fmt = SPNG_FMT_G8;
  }
  if (convert_manual)
  {
    fmt = SPNG_FMT_RGB8;
  }
  //  if (*outMono && *outHasAlpha )
  // {
  //   fmt = SPNG_FMT_GA8;
  // }

  spng_decode_image(ctx, *outData, out_size, fmt, 0);
  if (convert_manual)
  {
    unsigned char* temp_data = (unsigned char*) malloc(out_size);
    memcpy(temp_data,*outData,out_size);
    for (uint32_t i = 0; i < ihdr.width*ihdr.height;i++)
    {
      (*outData)[i] = temp_data[i*3];
    }

    free(temp_data);
  }

  /* Free context memory */
  spng_ctx_free(ctx);
  fclose(fp);
}

//void loadPngImage(const char *name, int* outWidth, int* outHeight, bool* outHasAlpha, GLubyte **outData,bool* outMono) {
//  // printf("%s\n",name );
//  int i;
//    png_structp png_ptr;
//    png_infop info_ptr;
//    unsigned int sig_read = 0;
//    int color_type, interlace_type;
//    FILE *fp = NULL;
//
//    if ((fp = th_fopen(name, "rb")) == NULL)
//        printf("%s %s\n","PNG error 1",name );
//
//    /* Create and initialize the png_struct
//     * with the desired error handler
//     * functions.  If you want to use the
//     * default stderr and longjump method,
//     * you can supply NULL for the last
//     * three parameters.  We also supply the
//     * the compiler header file version, so
//     * that we know if the application
//     * was compiled with a compatible version
//     * of the library.  REQUIRED
//     */
//    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
//                                     NULL, NULL, NULL);
//
//    if (png_ptr == NULL) {
//        fclose(fp);
//        printf("%s\n","PNG error 2" );
//    }
//
//    /* Allocate/initialize the memory
//     * for image information.  REQUIRED. */
//    info_ptr = png_create_info_struct(png_ptr);
//    if (info_ptr == NULL) {
//        fclose(fp);
//        png_destroy_read_struct(&png_ptr, NULL, NULL);
//        printf("%s\n","PNG error 3" );
//    }
//
//    /* Set error handling if you are
//     * using the setjmp/longjmp method
//     * (this is the normal method of
//     * doing things with libpng).
//     * REQUIRED unless you  set up
//     * your own error handlers in
//     * the png_create_read_struct()
//     * earlier.
//     */
//    if (setjmp(png_jmpbuf(png_ptr))) {
//        /* Free all of the memory associated
//         * with the png_ptr and info_ptr */
//        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
//        fclose(fp);
//        /* If we get here, we had a
//         * problem reading the file */
//        printf("%s\n","PNG error 4" );
//    }
//
//    /* Set up the output control if
//     * you are using standard C streams */
//    png_init_io(png_ptr, fp);
//
//    /* If we have already
//     * read some of the signature */
//    png_set_sig_bytes(png_ptr, sig_read);
//
//    /*
//     * If you have enough memory to read
//     * in the entire image at once, and
//     * you need to specify only
//     * transforms that can be controlled
//     * with one of the PNG_TRANSFORM_*
//     * bits (this presently excludes
//     * dithering, filling, setting
//     * background, and doing gamma
//     * adjustment), then you can read the
//     * entire image (including pixels)
//     * into the info structure with this
//     * call
//     *
//     * PNG_TRANSFORM_STRIP_16 |
//     * PNG_TRANSFORM_PACKING  forces 8 bit
//     * PNG_TRANSFORM_EXPAND forces to
//     *  expand a palette into RGB
//     */
//    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
//
//    png_uint_32 width, height;
//    int bit_depth;
//    int sucess = png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
//                 &interlace_type, NULL, NULL);
//                 // printf("%i\n",color_type  );
//    *outWidth = width;
//    *outHeight = height;
//    *outHasAlpha = false;
//    *outMono = false;
//    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GA)
//    *outHasAlpha = true;
//    if (color_type == PNG_COLOR_TYPE_GRAY)
//    {
//      *outMono = true;
//    }
//
//    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
//    *outData = (unsigned char*) malloc(row_bytes * height);
//
//    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
//
//    for (i = 0; i < *outHeight; i++) {
//        // note that png is ordered top to
//        // bottom, but OpenGL expect it bottom to top
//        // so the order or swapped
//        memcpy(*outData+(row_bytes * (height-1-i)), row_pointers[height-1-i], row_bytes);
//    }
//
//    /* Clean up after the read,
//     * and free any memory allocated */
//    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
//
//    /* Close the file */
//    fclose(fp);
//
//
//
//}

GLuint fn_loadTextureArray(const char** files,int numfiles)
{
  GLubyte** images = malloc(sizeof(GLubyte*)*numfiles);
  int width = 1;
  int height = 1;
   bool hasAlpha = false;
   bool hasMono = false;
  for (int i = 0 ; i < numfiles;i++)
  {



      printf("%s\n",files[i] );
     loadPngImage(files[i], &width, &height, &hasAlpha, &images[i],&hasMono,NULL);
     printf("%i %i\n",hasAlpha,hasMono );
  }
  GLuint ret;

  glGenTextures(1, &ret);
  glBindTexture(GL_TEXTURE_2D_ARRAY, ret);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
   glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
   glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
   glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

   glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,hasAlpha ? GL_RGBA8 : GL_RGB8,width,height,numfiles);

   for (int j = 0 ; j < numfiles;j++)
   {
     glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,j,width,height,1,hasAlpha ? GL_RGBA : GL_RGB,GL_UNSIGNED_BYTE,images[j]);
     free(images[j]);

     // glTexImage2D(GL_TEXTURE_2D, 0, hasAlpha ? GL_RGBA : GL_RGB, width,
     //              height, 0, hasAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
     //              textureImage);
   }

  glGenerateMipmap(GL_TEXTURE_2D_ARRAY);


  //no anti aliasing
  glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   // glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, fn_getProfileVar("texlodbias"));
  glBindTexture( GL_TEXTURE_2D_ARRAY, 0);
  free(images);
  return ret;
}



GLuint fn_loadTexture(const char* file)
{
  // if (!setbound)
  // {
  //   setbound = true;
  //   int i;
  //   for (i = 0 ; i < 32;i++)
  //   {
  //     boundtexttures[i][0] = -1;
  //     boundtexttures[i][1] = -1;
  //     boundtexttures[i][2] = -1;
  //   }
  // }
  GLuint ret;

  glGenTextures(1, &ret);
  glBindTexture(GL_TEXTURE_2D, ret);

  // glTexImage2D(GL_TEXTURE_2D, 0, Mode, Surface->w, Surface->h, 0, Mode, GL_UNSIGNED_BYTE, Surface->pixels);
  int width, height;
   bool hasAlpha;
   bool hasMono;

   GLubyte *textureImage;
   // printf("%s\n",file );
   loadPngImage(file, &width, &height, &hasAlpha, &textureImage,&hasMono,NULL);



   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   if (hasMono)
   {
     glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width,
                  height, 0, GL_RED, GL_UNSIGNED_BYTE,
                  textureImage);
   }
   else
   {
     glTexImage2D(GL_TEXTURE_2D, 0, hasAlpha ? GL_RGBA : GL_RGB, width,
                  height, 0, hasAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                  textureImage);
   }

    glGenerateMipmap(GL_TEXTURE_2D);
//    free(textureImage);

  //no anti aliasing
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   // glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, fn_getProfileVar("texlodbias"));
  glBindTexture( GL_TEXTURE_2D, 0);

  free(textureImage);
  // int newvarid = fn_setProfileVar(file,(float)texIndex);
  // pvars_alloced = realloc(pvars_alloced,sizeof(int)*(texIndex + 1));
  // pvars_alloced[texIndex] = newvarid;
  //
  // char* tmp = malloc(sizeof(char)*32);
  // sprintf(tmp,"%i",ret);
  // fn_setProfileVar(tmp,(float)hasAlpha);
  // texIndex++;
  return ret;
}

GLuint fn_loadWatermark(unsigned int width,unsigned int height, GLubyte* data)
{
  GLuint ret;

  glGenTextures(1, &ret);
  glBindTexture(GL_TEXTURE_2D, ret);

   bool hasAlpha = false;

   GLubyte *textureImage = data;




   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width,
                height, 0,  GL_RGB, GL_UNSIGNED_BYTE,
                textureImage);



    glGenerateMipmap(GL_TEXTURE_2D);
//    free(textureImage);

  //no anti aliasing
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
  // glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, fn_getProfileVar("texlodbias"));
  glBindTexture( GL_TEXTURE_2D, 0);

  free(textureImage);

  return ret;
}

typedef struct
{
  int width;
  int height;
  bool alpha;
  bool mono;
  GLubyte* data;
}th_PNGImage;

#define TH_MAX_IMAGES_IN_GROUP 500

typedef struct
{
  int side;
  th_PNGImage images[TH_MAX_IMAGES_IN_GROUP];
  int imagecount;
}th_SizeGroup;

void r_bindTexture(GLuint texture,GLenum type,int unit)
{
  if ( boundtexttures[unit][enumtoid(type)] == texture)
  {
    return;
  }


  boundtexttures[unit][enumtoid(type)] = texture;

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture( type, texture );
}

void r_bindTextureForce(GLuint texture,GLenum type,int unit)
{
  boundtexttures[unit][enumtoid(type)] = texture;

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture( type, texture );
}

static void loadRGBandMono()
{

}

typedef struct
{
  int start;
  int range;
  char** files;
  th_PNGImage* file_pngs;
  //th_Allocator* alloc;
}th_threadPngData;

static void thread_loadpngs(void* data)
{
  th_threadPngData* dat = (th_threadPngData*)data;
  char** files = dat->files;
  th_PNGImage* file_pngs = dat->file_pngs;
  for (int i =dat->start ; i< dat->start + dat->range;i++)
  {
    file_pngs[i].width = 0;
    file_pngs[i].data = NULL;
    if (strcmp(files[i],"") != 0)
    {
      //size_t alloc_size = 0;
      loadPngImage(files[i], &file_pngs[i].width, &file_pngs[i].height, &file_pngs[i].alpha, &file_pngs[i].data,&file_pngs[i].mono,NULL);
      //file_pngs[i].data = th_arenaManage(dat->alloc,file_pngs[i].data,alloc_size);
    }


  }
}

static fn_vec2* handles = NULL;
static int handles_count;
static GLuint* textures_last_loaded = NULL;
static GLuint num_textures_loaded = 0;
static bool init_gpu_texture_mem = false;
static GLuint* textures = NULL;

void r_allocateTextureHandles()
{
  handles = malloc(sizeof(fn_vec2)*1024);
  handles_count = 0;
}

typedef enum
{
  TH_BC4,
  TH_BC5,
  TH_BC7,
}th_EncodingScheme;

typedef struct
{
  th_EncodingScheme scheme;
  int side;
  int numimages;
  KTX_image* images;
}th_KTXGroup;

void th_loadMaterials(char** files,int filecount)
{
  // th_PNGImage* file_pngs = malloc(sizeof(th_PNGImage)*filecount);
  //
  // printf("Loading %i PNG images for %i materials\n",filecount,(filecount/5) );
  // TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_threadPngData,filecount)
  // data[th_thread_id].start = start_pos;
  // data[th_thread_id].range = add;
  // data[th_thread_id].files = files;
  // data[th_thread_id].file_pngs = file_pngs;
  // //data[th_thread_id].alloc = alloc;
  // TH_SCHEDULING_FUNC
  // th_setThread(thread_loadpngs,(void*)&data[th_thread_id],th_thread_id);
  // TH_END_SCHEDULING


  KTX_image* file_ktxs = malloc(sizeof(KTX_image)*filecount);
  printf("Loading %i KTX images for %i materials\n",filecount,(filecount/TEX_PER_MATERIAL) );
  for (int i = 0 ; i < filecount;i++ )
  {
    file_ktxs[i].width = 0;
    file_ktxs[i].data = NULL;
    if (strcmp(files[i],"") != 0)
    {
      th_loadKTX(files[i],&file_ktxs[i]);
    }
  }


  printf("Processing %i KTX images\n",filecount );
  //assume order Albedo,Metallic,Normal,Roughness,displacement
  //compress into two textures
  // th_PNGImage* compressed_pngs = malloc(sizeof(th_PNGImage)*(filecount/5)*3);
  // int pngcount = 0;
  // th_SizeGroup* groups = malloc(sizeof(th_SizeGroup)*256);
  // for (int i =0 ; i < 256;i++)
  // {
  //   groups[i].imagecount =0;
  // }
  // int groupCount = 0;
  int groupCount = 6;
  th_KTXGroup* groups = malloc(sizeof(th_KTXGroup)*6);
  groups[0].side = 2048;
  groups[0].scheme = TH_BC7;
  groups[0].numimages = 0;
  groups[0].images = malloc(sizeof(KTX_image)*500);

  groups[1].side = 2048;
  groups[1].scheme = TH_BC5;
  groups[1].numimages = 0;
  groups[1].images = malloc(sizeof(KTX_image)*500);

  groups[2].side = 2048;
  groups[2].scheme = TH_BC4;
  groups[2].numimages = 0;
  groups[2].images = malloc(sizeof(KTX_image)*500);

  groups[3].side = 128;
  groups[3].scheme = TH_BC7;
  groups[3].numimages = 0;
  groups[3].images = malloc(sizeof(KTX_image)*500);

  groups[4].side = 128;
  groups[4].scheme = TH_BC5;
  groups[4].numimages = 0;
  groups[4].images = malloc(sizeof(KTX_image)*500);

  groups[5].side = 128;
  groups[5].scheme = TH_BC4;
  groups[5].numimages = 0;
  groups[5].images = malloc(sizeof(KTX_image)*500);

  for (int i =0 ;i < filecount/TEX_PER_MATERIAL;i++)
  {
    //res group
    int group_offset = 0;
    if (file_ktxs[i*TEX_PER_MATERIAL + 0].header.pixelWidth == 2048)
    {
      group_offset = 0;
    }
    else if (file_ktxs[i*TEX_PER_MATERIAL + 0].header.pixelWidth == 128)
    {
      group_offset = 3;
    }
    else
    {
      printf("Invalid resolution %i, only 2048 and 128 accepted\n",file_ktxs[i*TEX_PER_MATERIAL + 0].header.pixelWidth);
      continue;
    }
    int into_group = 0;
    //albedmometal offset group 0
    th_KTXGroup* g = &groups[group_offset + into_group];
    g->images[g->numimages] = file_ktxs[i*TEX_PER_MATERIAL + 0];
    g->numimages = g->numimages + 1;
    int albmetalnormal_offset = g->numimages - 1;

    //normal offset group 1
    into_group = 1;
    g = &groups[group_offset + into_group];
    g->images[g->numimages] = file_ktxs[i*TEX_PER_MATERIAL + 1];
    g->numimages = g->numimages + 1;

    //roughness offset group 2
    into_group = 2;
    g = &groups[group_offset + into_group];
    g->images[g->numimages] = file_ktxs[i*TEX_PER_MATERIAL + 2];
    g->numimages = g->numimages + 1;
    int rough_offset = g->numimages - 1;

    //displacement/alpha mask (optional) offset group 2
    if (file_ktxs[i*TEX_PER_MATERIAL + 3].data != NULL)
    {
      into_group = 2;
      g = &groups[group_offset + into_group];
      g->images[g->numimages] = file_ktxs[i*TEX_PER_MATERIAL + 3];
      g->numimages = g->numimages + 1;
    }



    //world textures are 2048 and particles are always 128 so this is implicit in shader
    //AM/N count and then roughness/Disp count offset


    handles[handles_count] = fn_createVec2(albmetalnormal_offset,rough_offset);
    handles_count++;






  }


  if (!init_gpu_texture_mem)
  {
    textures = malloc(sizeof(GLuint)*groupCount);
  }

  for (int i =0 ; i < groupCount;i++)
  {
    //make a 3d texture
    printf("GROUP %i %i\n", groups[i].side,groups[i].numimages);
    if (groups[i].numimages == 0)
    {
      printf("Empty group!\n");
      continue;
    }

    if (groups[i].numimages > TEX_GROUP_IMG_SIZE)
    {
      printf("group limit too low\n");
      exit(0);
    }




     int mips = groups[i].images[0].header.numberOfMipmapLevels;
     uint32_t fmt = groups[i].images[0].header.glInternalFormat;

     if (!init_gpu_texture_mem)
     {
       glGenTextures(1, &textures[i]);
       glBindTexture(GL_TEXTURE_2D_ARRAY, textures[i]);

       glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
       glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
       glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
       glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
       glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
       glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, mips);
       if (groups[i].side == 128)
       {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
       }
       else
       {
         glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
         glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
       }





       glTexStorage3D(GL_TEXTURE_2D_ARRAY,mips,fmt,groups[i].side,groups[i].side,TEX_GROUP_IMG_SIZE);


     }
     else
     {
       glBindTexture(GL_TEXTURE_2D_ARRAY, textures[i]);
     }




     for (int j = 0 ; j < groups[i].numimages;j++)
     {
       //glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,j,groups[i].side,groups[i].side,1,GL_RGBA,GL_UNSIGNED_BYTE,groups[i].images[j].data);
       if (groups[i].images[j].header.glInternalFormat != fmt)
       {
          printf("Format error mistmatch\n");
       }

       for (int k = 0 ; k < mips;k++)
       {
         uint32_t width  = fmax(1, groups[i].side >> k);
         uint32_t height = fmax(1, groups[i].side >> k);


         glCompressedTexSubImage3D(
           GL_TEXTURE_2D_ARRAY,
           k,                          // mip level
           0, 0, j,                   // offsets
           width, height, 1,
           groups[i].images[j].header.glInternalFormat,             // same format
           groups[i].images[j].data_sizes[k],        // compressed size
           groups[i].images[j].data[k]               // compressed data
         );
       }



       //free(groups[i].images[j].data);
     }

    //glGenerateMipmap(GL_TEXTURE_2D_ARRAY);


    //no anti aliasing
    glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    // glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  //  glTexParameterf(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_LOD_BIAS,1.0);
    // glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 6);
    glBindTexture( GL_TEXTURE_2D_ARRAY, 0);


  }
  for (int i =0 ; i < groupCount;i++)
  {
    r_bindTexture(textures[i],GL_TEXTURE_2D_ARRAY,i + 6);
  }
//
  for (int i = 0 ; i < filecount;i++)
  {
    if (file_ktxs[i].data != NULL)
    {
      for (uint32_t j = 0 ; j < file_ktxs[i].header.numberOfMipmapLevels;j++)
      {
        free(file_ktxs[i].data[j]);
      }
      free(file_ktxs[i].data);
      free(file_ktxs[i].data_sizes);
    }


  }

  free(file_ktxs);
  //free(compressed_pngs);

  for (int i = 0 ; i < groupCount;i++)
  {
    free(groups[i].images);
  }
  free(groups);

  //dont free textures, we need their handles later to free them
  textures_last_loaded = NULL;
  num_textures_loaded = groupCount;

  init_gpu_texture_mem = true;


}

void th_unloadMaterials()
{
  // if (textures_last_loaded != NULL)
  // {
  //   // for (unsigned int i = 0 ; i < num_textures_loaded;i++ )
  //   // {
  //   //   r_bindTexture(0,GL_TEXTURE_2D_ARRAY,i + 6);
  //   //   glDeleteTextures(1,&textures_last_loaded[i]);
  //   // }
  //   free(textures_last_loaded);
  //   textures_last_loaded = NULL;
  // }
  handles_count = 0;
}

fn_vec2 th_getMaterialHandle(int index)
{
  return handles[index];
}



typedef struct
{
  int texID;
  GLuint* remapping;
}texmapping_t;

static texmapping_t* mappings = NULL;
static int mappingsCount = 0;
void r_markTextureRemapping(int texID,GLuint* remapping)
{
  mappings = realloc(mappings,sizeof(texmapping_t)*(mappingsCount + 1));
  mappings[mappingsCount].texID = texID;
  mappings[mappingsCount].remapping = remapping;
  mappingsCount++;
}

GLuint* r_isTextureRemapped(int texID)
{
  int i;
  for (i = 0 ; i < mappingsCount;i++)
  {
    if (mappings[i].texID == texID)
    {
      return mappings[i].remapping;
    }
  }
  return NULL;
}


char* th_getTextureFromMtl(const char* file)
{

  FILE* fptr = NULL;
  fptr = th_fopen(file,"r");
  char* final = NULL;
  if (fptr)
  {
    while( 1 ){

      char lineHeader[1024];

      // read the first word of the line
      int res = fscanf(fptr, "%s", lineHeader);
    //    printf("%s\n",lineHeader );
      if (res == EOF)
      break; // EOF = End Of File. Quit the loop.
      if ( strcmp( lineHeader, "map_Kd" ) == 0 ){

        char fname[1024];
        fscanf(fptr, "%s", fname);
        char* substr = strstr(fname,"albedo.png");
        if (substr != NULL)
        {
          int iter = 0;
          substr -= 2;
          while(*substr != '/')
          {
            substr--;
            iter++;
          }
          substr++;
          iter++;
          char* ret = malloc(sizeof(char)*1024);
          memcpy(ret,substr,sizeof(char)*iter);
          ret[iter] = '\0';
          final = ret;

        }

      }
    }
  }

  if (final == NULL)
  {
    const char* temp = "cubemap/";
    char* ret = malloc(sizeof(char)*1024);
    memcpy(ret,temp,sizeof("cubemap/"));
    final = ret;
  }

  fclose(fptr);
  return final;
}
