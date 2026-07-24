#include "../fn_math/fn_math.h"
#include "r_vertex.h"
#include "th_gpu.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "r_mesh.h"
#include "th_defines.h"
#include "../th_fopen.h"

#define SUB(a,b) fn_addVec3(a,fn_multVec3(b,fn_createVec3(-1,-1,-1)))
#define ADD(a,b) fn_addVec3(a,b)
#define MUL(a,b) fn_multVec3(a,b)
#define DIV(a,b) fn_multVec3(a,fn_createVec3(1.f/b.x,1.f/b.y,1.f/b.z))
#define V3(a,b,c) fn_createVec3(a,b,c)
#define V3S(a) fn_createVec3(a,a,a)
// static bool PRESERVE_NORMALS = false;
// static bool equalVert(fn_Vertex a,fn_Vertex b)
// {
//   if (fn_equalVec3(a.position,b.position) && fn_equalVec2(a.texCoord,b.texCoord) && (!PRESERVE_NORMALS || fn_equalVec3(a.normal,b.normal) ))//
//   {
//     return true;
//   }
//   return false;
// }

static bool use_pos_only = false;

static bool equalVertTh(th_Vertex a,th_Vertex b)
{
  //
  if (use_pos_only && fn_equalVec3(a.position,b.position))
  {
    return true;
  }
  if (fn_equalVec3(a.position,b.position)&& fn_equalVec3(a.normal,b.normal)&& fn_equalVec2(a.texCoord,b.texCoord) )//
  {
    return true;
  }
  return false;
}



// static bool equalPosVert(fn_Vertex a,fn_Vertex b)
// {
//   if (fn_equalVec3(a.position,b.position)   )//
//   {
//     return true;
//   }
//   return false;
// }
//
// static bool equalPosVertAnim(fn_AnimVertex a,fn_AnimVertex b)
// {
//   if (fn_equalVec3(a.position,b.position) && fn_equalVec3(a.position2,b.position2) )//
//   {
//     return true;
//   }
//   return false;
// }





// void r_scaleMesh(fn_Vertex* verts,int vertcount,fn_vec3 s)
// {
//   int i;
//   for (i = 0;i<vertcount;i++)
//   {
//     verts[i].position = MUL(verts[i].position,s);
//   }
// }

static void r_scaleMeshTh(th_Vertex* verts,int vertcount,fn_vec3 s)
{
  int i;
  for (i = 0;i<vertcount;i++)
  {
    verts[i].position = MUL(verts[i].position,s);
  }
}

// void r_translateMesh(fn_Vertex* verts,int vertcount,fn_vec3 t)
// {
//   int i;
//   for (i = 0;i<vertcount;i++)
//   {
//     verts[i].position = ADD(verts[i].position,t);
//   }
// }

void r_translateMeshTh(th_Vertex* verts,int vertcount,fn_vec3 t)
{
  int i;
  for (i = 0;i<vertcount;i++)
  {
    verts[i].position = ADD(verts[i].position,t);
  }
}
/*
fn_vec3 r_centerMesh(fn_Vertex* verts,int vertcount)
{
  int i;
  fn_vec3 min = verts[0].position;
  fn_vec3 max = verts[0].position;

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x < min.x)
      min.x = verts[i].position.x;
    if(verts[i].position.y < min.y)
      min.y = verts[i].position.y;
    if(verts[i].position.z < min.z)
      min.z = verts[i].position.z;
  }

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x > max.x)
      max.x = verts[i].position.x;
    if(verts[i].position.y > max.y)
      max.y = verts[i].position.y;
    if(verts[i].position.z > max.z)
      max.z = verts[i].position.z;
  }


  fn_vec3 dif = MUL(ADD(max,min),V3S(-0.5));
  r_translateMesh(verts,vertcount,dif);
  return dif;
}*/

fn_vec3 r_centerMeshTh(th_Vertex* verts,int vertcount)
{
  if (vertcount == 0)
  {
    return fn_createVec3(0,0,0);
  }
  int i;
  fn_vec3 min = verts[0].position;
  fn_vec3 max = verts[0].position;

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x < min.x)
      min.x = verts[i].position.x;
    if(verts[i].position.y < min.y)
      min.y = verts[i].position.y;
    if(verts[i].position.z < min.z)
      min.z = verts[i].position.z;
  }

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x > max.x)
      max.x = verts[i].position.x;
    if(verts[i].position.y > max.y)
      max.y = verts[i].position.y;
    if(verts[i].position.z > max.z)
      max.z = verts[i].position.z;
  }


  fn_vec3 dif = MUL(ADD(max,min),V3S(-0.5));
  r_translateMeshTh(verts,vertcount,dif);
  return dif;
}

fn_vec3 th_getCenterVerts(th_GpuData* data)
{
  th_Vertex* verts = data->verts;
  int vertcount  = data->vertcount;

  if (vertcount == 0)
  {
    return fn_createVec3(0,0,0);
  }
  int i;
  // fn_vec3 min = verts[0].position;
  // fn_vec3 max = verts[0].position;

  fn_vec3 sum = fn_createVec3(0,0,0);

  for (i =0;i < vertcount;i++ )
  {
    // if(verts[i].position.x < min.x)
    //   min.x = verts[i].position.x;
    // if(verts[i].position.y < min.y)
    //   min.y = verts[i].position.y;
    // if(verts[i].position.z < min.z)
    //   min.z = verts[i].position.z;
    sum = fn_addVec3(sum,verts[i].position);
  }

  // for (i =0;i < vertcount;i++ )
  // {
  //   if(verts[i].position.x > max.x)
  //     max.x = verts[i].position.x;
  //   if(verts[i].position.y > max.y)
  //     max.y = verts[i].position.y;
  //   if(verts[i].position.z > max.z)
  //     max.z = verts[i].position.z;
  // }
  sum = fn_multVec3s(sum,1.0/((float)vertcount));

  return sum;

  // fn_vec3 dif = MUL(ADD(max,min),V3S(0.5));
  // return dif;
}


void th_getMinMaxVerts(th_GpuData* data,fn_vec3* minout,fn_vec3* maxout)
{
  th_Vertex* verts = data->verts;
  int vertcount  = data->vertcount;

  if (vertcount == 0)
  {
    *minout = fn_createVec3(0,0,0);
    *maxout = fn_createVec3(0,0,0);
  }
  int i;
  fn_vec3 min = verts[0].position;
  fn_vec3 max = verts[0].position;

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x < min.x)
      min.x = verts[i].position.x;
    if(verts[i].position.y < min.y)
      min.y = verts[i].position.y;
    if(verts[i].position.z < min.z)
      min.z = verts[i].position.z;
  }

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x > max.x)
      max.x = verts[i].position.x;
    if(verts[i].position.y > max.y)
      max.y = verts[i].position.y;
    if(verts[i].position.z > max.z)
      max.z = verts[i].position.z;
  }

  *minout = min;
  *maxout = max;
}

// fn_NormalizeTransform r_normalizeMesh(fn_Vertex* verts,int vertcount)
// {
//   int i;
//   fn_vec3 min = verts[0].position;
//   fn_vec3 max = verts[0].position;
//
//
//   fn_vec3 nmin = fn_createVec3(0,0,0);
//   fn_vec3 nmax = fn_createVec3(1,1,1);
//
//   for (i =0;i < vertcount;i++ )
//   {
//     if(verts[i].position.x < min.x)
//       min.x = verts[i].position.x;
//     if(verts[i].position.y < min.y)
//       min.y = verts[i].position.y;
//     if(verts[i].position.z < min.z)
//       min.z = verts[i].position.z;
//   }
//
//   for ( i =0;i < vertcount;i++ )
//   {
//     if(verts[i].position.x > max.x)
//       max.x = verts[i].position.x;
//     if(verts[i].position.y > max.y)
//       max.y = verts[i].position.y;
//     if(verts[i].position.z > max.z)
//       max.z = verts[i].position.z;
//   }
//   float s = fmin(fmin((nmax.x-nmin.x)/(max.x-min.x),
//   (nmax.y-nmin.y)/(max.y-min.y)),
//   (nmax.z-nmin.z)/(max.z-min.z));
//
//   for ( i =0;i < vertcount;i++)
//   {
//     verts[i].position=ADD(MUL(ADD(verts[i].position,MUL(V3(-0.5,-0.5,-0.5),ADD(min,max))),V3S(s)),MUL(V3S(0.5),ADD(nmin,nmax)));
//   }
//   fn_NormalizeTransform t;
//   t.min = min;
//   t.max = max;
//   t.s = s;
//   return t;
//
// }

void r_normalizeMeshTh(th_Vertex* verts,int vertcount)
{
  if (vertcount == 0)
  {
    return;
  }
  int i;
  fn_vec3 min = verts[0].position;
  fn_vec3 max = verts[0].position;


  fn_vec3 nmin = fn_createVec3(0,0,0);
  fn_vec3 nmax = fn_createVec3(1,1,1);

  for (i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x < min.x)
      min.x = verts[i].position.x;
    if(verts[i].position.y < min.y)
      min.y = verts[i].position.y;
    if(verts[i].position.z < min.z)
      min.z = verts[i].position.z;
  }

  for ( i =0;i < vertcount;i++ )
  {
    if(verts[i].position.x > max.x)
      max.x = verts[i].position.x;
    if(verts[i].position.y > max.y)
      max.y = verts[i].position.y;
    if(verts[i].position.z > max.z)
      max.z = verts[i].position.z;
  }
  float s = fmin(fmin((nmax.x-nmin.x)/(max.x-min.x),
  (nmax.y-nmin.y)/(max.y-min.y)),
  (nmax.z-nmin.z)/(max.z-min.z));

  for ( i =0;i < vertcount;i++)
  {
    verts[i].position=ADD(MUL(ADD(verts[i].position,MUL(V3(-0.5,-0.5,-0.5),ADD(min,max))),V3S(s)),MUL(V3S(0.5),ADD(nmin,nmax)));
  }


}





static bool containsv3(fn_vec3* indices,fn_vec3 index,int range)
{
  int  i;
  bool ret = false;

  for (i = 0 ;i < range;i++)
  {
    if (fn_equalVec3(indices[i], index))
    {
      ret = true;
    }
  }
  return ret;
}

static fn_vec3 fn_calculateSurfaceNormal (fn_vec3 points[3])
{
	fn_vec3 U = fn_subVec3(points[1] , points[0]);
	fn_vec3 V = fn_subVec3(points[2] , points[0]);

  fn_vec3 normal;
	normal.x = (U.y * V.z) - (U.z * V.y);
	normal.y = (U.z * V.x) - (U.x * V.z);
	normal.z = (U.x * V.y) - (U.y * V.x);

	return fn_normalizeVec3(normal);

}

#define MAX_PLANES 600
static fn_vec3 normals[MAX_PLANES];
static int normalCount = 0;
static fn_vec3 centers[MAX_PLANES];
static int centerCount = 0;

static void checkPolygon(fn_Vertex* verts,GLuint* indices, int indCount)
{
  int temp[MAX_PLANES];
  int tempCount = 0;

  fn_vec3 curNormal = fn_createVec3(0,0,0);

  int i;
  for ( i = 0 ; i < indCount;i += 3)
  {
    fn_Vertex vert = verts[indices[i]];
    fn_vec3 points[3];
    points[0] = verts[indices[i]].position;
    points[1] = verts[indices[i + 1]].position;
    points[2] = verts[indices[i + 2]].position;
    fn_vec3 normal = fn_calculateSurfaceNormal(points);
    if (!containsv3(normals,normal,normalCount) && fn_equalVec3(curNormal,fn_createVec3s(0.f)))
    {

      normals[normalCount] = normal;
      normalCount++;
      curNormal = normal;
      temp[tempCount] = i;
      temp[tempCount + 1] = i + 1;
      temp[tempCount + 2] = i + 2;
      tempCount += 3;

    }
    else if ((fn_equalVec3(curNormal,normal)))
    {

      temp[tempCount] = i;
      temp[tempCount + 1] = i + 1;
      temp[tempCount + 2] = i + 2;
      tempCount += 3;
    }
  }
  if (tempCount > 0)
  {
    fn_vec3 sum = fn_createVec3(0,0,0);
    for (i = 0 ;i < tempCount;i++)
    {
      fn_Vertex vert = verts[indices[temp[i]]];
      sum = fn_addVec3(sum,vert.position);
    }
    sum = fn_divVec3(sum,fn_createVec3s(tempCount));
    centers[centerCount] = sum;
    centerCount++;
    checkPolygon(verts,indices,indCount);
  }
}




static void calcTangent(fn_vec3 normal,fn_vec3 pos1,fn_vec3 pos2,fn_vec3 pos3,fn_vec2 uv1,fn_vec2 uv2,fn_vec2 uv3,float* out)
{
  fn_vec3 edge1 = fn_subVec3(pos2 , pos1);
  fn_vec3 edge2 = fn_subVec3(pos3 , pos1);
  fn_vec2 deltaUV1 = fn_subVec2(uv2 , uv1);
  fn_vec2 deltaUV2 = fn_subVec2(uv3 , uv1);
  fn_vec3 tangent1,bitangent1;
  float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
  tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
  tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
  tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
  tangent1 = fn_normalizeVec3(tangent1);



  bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
  bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
  bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
  bitangent1 = fn_normalizeVec3(bitangent1);


  tangent1 = fn_normalizeVec3(SUB(tangent1 , fn_multVec3s(normal , fn_dot(normal, tangent1))));

  if (fn_dot(fn_cross(normal, tangent1), bitangent1) < 0.0f){
     tangent1 = fn_multVec3s(tangent1,-1);
 }
  out[0] = tangent1.x;
  out[1] = tangent1.y;
  out[2] = tangent1.z;
  out[3] = bitangent1.x;
  out[4] = bitangent1.y;
  out[5] = bitangent1.z;
}

typedef struct
{
  int face1;
  int face2;
  int vertex1;
  int vertex2;
}th_EdgePoint;

static fn_vec3 calcWinding(fn_vec3 p1,fn_vec3 p2,fn_vec3 p3)
{
  return fn_normalizeVec3(fn_cross(fn_subVec3(p2,p1),fn_subVec3(p3,p1)));
}

th_GpuData r_loadThorMesh(const char* filename,bool normalize,bool ignoreTexCoord,bool center_mesh)
{
  GLuint* indices;
  int verts;
  int inds;
//  int alloced = FN_MAX_OBJ*2;
  int i,j;
  int alloced[4] = {FN_MAX_OBJ*2,FN_MAX_OBJ*2,FN_MAX_OBJ*2,FN_MAX_OBJ*2};
  fn_vec3* temp_vertices = malloc(sizeof(fn_vec3)*FN_MAX_OBJ*2);
  fn_vec2* temp_uvs = malloc(sizeof(fn_vec2)*FN_MAX_OBJ*2);
  fn_vec3* temp_normals = malloc(sizeof(fn_vec3)*FN_MAX_OBJ*2);
  int size_verts = 0;
  int size_uvs = 0;
  int size_normals = 0;
  bool vertsparesed = false;

  unsigned int* vertexIndices = malloc(sizeof(unsigned int)*FN_MAX_OBJ_ALLOC*2);
  int size_vertexIndices = 0;
  unsigned int* uvIndices = malloc(sizeof(unsigned int)*FN_MAX_OBJ_ALLOC*2);
  int size_uvIndices = 0;
  unsigned int* normalIndices = malloc(sizeof(unsigned int)*FN_MAX_OBJ_ALLOC*2);
  int size_normalIndices = 0;

  int faces = 0;
  th_Vertex* data;
  th_Vertex* temp_data;
  FILE * file = th_fopen(filename, "r");
  if( file == NULL ){
    printf("Impossible to open the file ! %s\n",filename);
    th_GpuData r = {0};
    return r;
  }
  while( 1 ){

    char lineHeader[128];
    // read the first word of the line
    int res = fscanf(file, "%s", lineHeader);
    if (res == EOF)
    break; // EOF = End Of File. Quit the loop.
    if ( strcmp( lineHeader, "v" ) == 0 ){
      fn_vec3 vertex;
      fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
      if (alloced[0] <= size_verts)
      {
        temp_vertices = realloc(temp_vertices,sizeof(fn_vec3)*alloced[0]*2);
        alloced[0] =alloced[0]*2;
      }
      temp_vertices[size_verts] = vertex;
      size_verts++;

    }else if ( strcmp( lineHeader, "vt" ) == 0 ){
      fn_vec2 uv;
      fscanf(file, "%f %f\n", &uv.x, &uv.y );
      if (alloced[1] <= size_uvs)
      {
        temp_uvs = realloc(temp_uvs,sizeof(fn_vec2)*alloced[1]*2);
        alloced[1] =alloced[1]*2;
      }
      temp_uvs[size_uvs] = uv;
      size_uvs++;
    }else if ( strcmp( lineHeader, "vn" ) == 0 ){
      fn_vec3 normal;
      fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z );
      if (alloced[2] <= size_normals)
      {
        temp_normals = realloc(temp_normals,sizeof(fn_vec3)*alloced[2]*2);
        alloced[2] =alloced[2]*2;
      }
      temp_normals[size_normals] = normal;
      size_normals++;
    }
    else if ( strcmp( lineHeader, "f" ) == 0 ){
      if (!vertsparesed)
      {
        vertsparesed = true;
        // temp_uvs = realloc(temp_uvs,sizeof(fn_vec2)*size_uvs);
        // temp_vertices = realloc(temp_vertices,sizeof(fn_vec3)*size_verts);
        // temp_normals = realloc(temp_normals,sizeof(fn_vec3)*size_normals);
      }
       int vertexIndex[3], uvIndex[3], normalIndex[3];
      int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2] );
      if (matches != 9){
        printf("File %s can't be read Try exporting with other options\n",filename);
        fclose(file);
        th_GpuData r = {0};
        return r;
      }
      faces++;
      if (alloced[3] <= size_vertexIndices)
      {
        vertexIndices = realloc(vertexIndices,sizeof(unsigned int)*alloced[3]*2);
        uvIndices = realloc(uvIndices,sizeof(unsigned int)*alloced[3]*2);
        normalIndices = realloc(normalIndices,sizeof(unsigned int)*alloced[3]*2);
        alloced[3] =alloced[3]*2;
      }
    //  printf("%i/%i/%i %i/%i/%i %i/%i/%i\n",vertexIndex[0],uvIndex[0],normalIndex[0],vertexIndex[1],uvIndex[1],normalIndex[1],vertexIndex[2],uvIndex[2],normalIndex[2]);
      vertexIndices[size_vertexIndices] = vertexIndex[0];
      size_vertexIndices++;
      vertexIndices[size_vertexIndices] = vertexIndex[1];
      size_vertexIndices++;
      vertexIndices[size_vertexIndices] = vertexIndex[2];
      size_vertexIndices++;

      uvIndices[size_uvIndices] = uvIndex[0];
      size_uvIndices++;
      uvIndices[size_uvIndices] = uvIndex[1];
      size_uvIndices++;
      uvIndices[size_uvIndices] = uvIndex[2];
      size_uvIndices++;

      normalIndices[size_normalIndices] = normalIndex[0];
      size_normalIndices++;
      normalIndices[size_normalIndices] = normalIndex[1];
      size_normalIndices++;

      normalIndices[size_normalIndices] = normalIndex[2];
      size_normalIndices++;




    }
  }
  // vertexIndices = realloc(vertexIndices,sizeof(unsigned int)*size_vertexIndices);
  // uvIndices = realloc(uvIndices,sizeof(unsigned int)*size_uvIndices);
  // normalIndices = realloc(normalIndices,sizeof(unsigned int)*size_normalIndices);

  temp_data = malloc(faces*3*sizeof(th_Vertex));
  for (i = 0;i < faces*3;i++)
  {
    unsigned int vertexIndex = vertexIndices[i];
    temp_data[i].position = temp_vertices[ vertexIndex-1 ];
    unsigned int uvIndex = uvIndices[i];
    temp_data[i].texCoord = temp_uvs[ uvIndex-1 ];
    unsigned int normalIndex = normalIndices[i];
    temp_data[i].normal = temp_normals[ normalIndex-1 ];


  }
  for (i =0 ;i < faces*3;i+=3)
  {
    float in[6];
    unsigned int vertexIndex = vertexIndices[i] - 1;
    unsigned int uvIndex = uvIndices[i] - 1;
    calcTangent(temp_data[i].normal,temp_data[i].position,temp_data[i+1].position,temp_data[i + 2].position,temp_data[i].texCoord,temp_data[i+1].texCoord,temp_data[i+2].texCoord,in);
    temp_data[i].tangent = fn_createVec3(in[0],in[1],in[2]);
    temp_data[i].bitangent = fn_createVec3(in[3],in[4],in[5]);

    temp_data[i+1].tangent = fn_createVec3(in[0],in[1],in[2]);
    temp_data[i+1].bitangent = fn_createVec3(in[3],in[4],in[5]);


    temp_data[i+2].tangent = fn_createVec3(in[0],in[1],in[2]);
    temp_data[i+2].bitangent = fn_createVec3(in[3],in[4],in[5]);
  }
  if (normalize)
    r_normalizeMeshTh(temp_data,faces*3);

  //fn_vec3 d= fn_createVec3s(0);
  if (center_mesh)
    r_centerMeshTh(temp_data,faces*3);

  r_scaleMeshTh(temp_data,faces*3,V3S(FN_UNIT));

  // if (dif != NULL)
  // {
  //   *dif = d;
  // }
  // if (t != NULL)
  // {
  //   *t = tr;
  // }


  // for ( i =0;i < faces*3;i++)
  // (*indices)[i] = i;

  //create extra faces for texture seams
  if (ignoreTexCoord)
  {
    th_EdgePoint* edges = malloc(sizeof(th_EdgePoint)*faces*3);
    int edgeCount = 0;
    for (int i = 0;i < faces*3;i++)//foreach vertex
    {
      th_Vertex v1 = temp_data[i];
      //has this position already been added with another texcoord
      for (int j = 0; j < i;j++)
      {
        th_Vertex v2 = temp_data[j];
        if ( fn_equalVec3(v1.position,v2.position) && !fn_equalVec2(v1.texCoord,v2.texCoord))
        {
          edgeCount++;
          edges[edgeCount - 1].vertex1 = i;
          edges[edgeCount - 1].vertex2 = j;
          edges[edgeCount - 1].face1 = floor(i/3.0);
          edges[edgeCount - 1].face2 = floor(j/3.0);
        }
      }
    }

    th_Vertex* new_verts = malloc(sizeof(th_Vertex)*edgeCount*6);
    int new_vert_count = 0;
    //foreach edgepoint
    for (int i = 0 ; i  < edgeCount;i++)
    {
      //see if a previous one has the same 2 faces
      for (int j = 0 ; j < i;j++)
      {
        fn_vec3 edgei_pos = temp_data[edges[i].vertex1].position;
        fn_vec3 edgej_pos = temp_data[edges[j].vertex1].position;
        if (!fn_equalVec3(edgei_pos,edgej_pos))
        {


        if ((edges[i].face1 == edges[j].face1 && edges[i].face2 == edges[j].face2))
        {

         th_Vertex a1 = temp_data[edges[i].vertex1];
         th_Vertex a2 = temp_data[edges[i].vertex2];

         th_Vertex b1 = temp_data[edges[j].vertex1];
         th_Vertex b2 = temp_data[edges[j].vertex2];

           new_verts[new_vert_count + 2] = a1;
           new_verts[new_vert_count + 1] = b1;
           new_verts[new_vert_count + 0] = a2;
           new_verts[new_vert_count + 5] = b1;
           new_verts[new_vert_count + 4] = b2;
           new_verts[new_vert_count + 3] = a2;
         // }
         // else
         // {
           new_verts[new_vert_count + 6] = a1;
           new_verts[new_vert_count + 7] = b1;
           new_verts[new_vert_count + 8] = a2;
           new_verts[new_vert_count + 9] = b1;
           new_verts[new_vert_count + 10] = b2;
           new_verts[new_vert_count + 11] = a2;
         // }

         new_vert_count+=12;
        }
        // else if ((edges[i].face1 == edges[j].face2 && edges[i].face2 == edges[j].face1))
        // {
        //   printf("%s\n","b" );
        //   th_Vertex a1 = temp_data[edges[i].vertex1];
        //   th_Vertex a2 = temp_data[edges[i].vertex2];
        //
        //   th_Vertex b1 = temp_data[edges[j].vertex1];
        //   th_Vertex b2 = temp_data[edges[j].vertex2];
        //   new_verts[new_vert_count + 0] = a1;
        //   new_verts[new_vert_count + 1] = b1;
        //   new_verts[new_vert_count + 2] = a2;
        //   new_verts[new_vert_count + 3] = a2;
        //   new_verts[new_vert_count + 4] = b1;
        //   new_verts[new_vert_count + 5] = b2;
        //   new_vert_count+=6;
        // }
        // else if ((edges[i].face2 == edges[j].face1 && edges[i].face1 == edges[j].face2))
        // {
        //   printf("%s\n","c" );
        //   th_Vertex a1 = temp_data[edges[i].vertex1];
        //   th_Vertex a2 = temp_data[edges[i].vertex2];
        //
        //   th_Vertex b1 = temp_data[edges[j].vertex1];
        //   th_Vertex b2 = temp_data[edges[j].vertex2];
        //   new_verts[new_vert_count + 0] = a1;
        //   new_verts[new_vert_count + 1] = b1;
        //   new_verts[new_vert_count + 2] = a2;
        //   new_verts[new_vert_count + 3] = a2;
        //   new_verts[new_vert_count + 4] = b1;
        //   new_verts[new_vert_count + 5] = b2;
        //   new_vert_count+=6;
        // }
      }
      }
    }
  //  printf("%i\n", new_vert_count);
    int old_faces = faces;
    faces += new_vert_count/3;
    temp_data = realloc(temp_data,(faces*3)*sizeof(th_Vertex));
    memcpy(&temp_data[old_faces*3],new_verts,new_vert_count*sizeof(th_Vertex));
    free(new_verts);
    free(edges);
  }
  verts = 0;
  data = malloc(faces*3*2*sizeof(th_Vertex));

  inds = 0;
  indices = malloc(faces*3*2*sizeof(GLuint));
  for (i = 0;i < faces*3;i++)
  {
    bool found = false;
    int vert;
    for (j = 0;j < verts;j++)
    {
      if (equalVertTh(temp_data[i],data[j]))
      {
        found = true;
        vert = j;
      }
      // else if (ignoreTexCoord && fn_equalVec3(temp_data[i].position,data[j].position) && fn_equalVec3(temp_data[i].normal,data[j].normal))
      // {
      //   found = true;
      //   vert = j;
      // }

    }
    if (found)
    {
      inds = inds + 1;
      // *indices = realloc(*indices,sizeof(GLuint)*(*inds));
      (indices)[inds - 1] = vert;
      data[vert].tangent = fn_addVec3(data[vert].tangent,temp_data[i].tangent);
      data[vert].bitangent = fn_addVec3(data[vert].bitangent,temp_data[i].bitangent);
    }
    else
    {
      verts = verts + 1;
      // data = realloc(data,sizeof(fn_Vertex)*(*verts));
      data[verts - 1] = temp_data[i];
      inds = inds + 1;
      // *indices = realloc(*indices,sizeof(GLuint)*(*inds));
      (indices)[inds - 1] = verts - 1;

    }
  }




  data = realloc(data,sizeof(th_Vertex)*(verts));
  indices = realloc(indices,sizeof(GLuint)*(inds));

  free(vertexIndices);
  free(uvIndices);
  free(normalIndices);
  free(temp_uvs);
  free(temp_normals);
  free(temp_vertices);
  free(temp_data);
  fclose(file);
  th_GpuData ret;
  ret.verts = data;
  ret.indices = indices;
  ret.instances = NULL;
  ret.vertcount = verts;
  ret.indicecount = inds;
  ret.instancecount = 0;
  return ret;
}

void r_scaleMeshUVs(th_GpuData* data,fn_vec2 uv)
{
  for (GLuint i = 0 ; i < data->vertcount;i++)
  {
    data->verts[i].texCoord = fn_multVec2(data->verts[i].texCoord,uv);
  }
}

void r_scaleThorMesh(th_GpuData* data,fn_vec3 scale)
{
  for (GLuint i = 0 ; i < data->vertcount;i++)
  {
    data->verts[i].position = fn_multVec3(data->verts[i].position,scale);
  }
}

void r_translateThorMesh(th_GpuData* data,fn_vec3 translate)
{
  for (GLuint i = 0 ; i < data->vertcount;i++)
  {
    data->verts[i].position = fn_addVec3(data->verts[i].position,translate);
  }
}

void r_transformThorMesh(th_GpuData* data,fn_mat4 t)
{
  for (GLuint i = 0 ; i < data->vertcount;i++)
  {
    data->verts[i].position = fn_transformVec3(data->verts[i].position,t);
    data->verts[i].tangent = fn_transformNormal(data->verts[i].tangent,t);
    data->verts[i].normal = fn_transformNormal(data->verts[i].normal,t);
  }
}

fn_vec3* th_loadMeshVerts(const char* filename,unsigned int** indices,int* tricount,bool normalize,int* vcount,bool center_mesh)
{
  use_pos_only = true;
  th_GpuData d = r_loadThorMesh(filename,normalize,false,center_mesh);
  use_pos_only = false;
  *indices = d.indices;
  *vcount = d.vertcount;
  *tricount = d.indicecount;
  fn_vec3* vs = malloc(sizeof(fn_vec3)*(d.vertcount));
  for (GLuint i = 0 ; i < d.vertcount;i++)
  {
    vs[i] = d.verts[i].position;
  }
  free(d.verts);
  return vs;
}

// void th_loadMeshes(char** files,int count,th_GpuData* out,th_MeshFlag* flags)
// {
//   #pragma omp parallel for
//   for (int i = 0 ; i < count;i++)
//   {
//     out[i] = r_loadThorMesh(files[i],flags[i] & TH_NORMALIZEMESH,flags[i] & TH_IGNORETEXCOORD,true);
//   }
//
// }

void th_exportMWADs(th_GpuData* data,int count,const char* directory,const char* prefix,uint8_t* hash)
{
  for (int i = 0 ; i < count;i++)
  {
    char filename[1024];
    sprintf(filename,"%s%s%i.mwad",directory,prefix,i);
    FILE* mwad = th_fopen(filename,"wb");
    if (mwad == NULL)
    {
      printf("%s\n","CANNOT OPEN" );
    }
    fwrite(hash,sizeof(uint8_t),16,mwad);
    fwrite(&data[i].vertcount,sizeof(GLuint),1,mwad);
    fwrite(&data[i].indicecount,sizeof(GLuint),1,mwad);
    fwrite(&data[i].instancecount,sizeof(GLuint),1,mwad);
    fwrite(data[i].verts,sizeof(th_Vertex),data[i].vertcount,mwad);
    fwrite(data[i].indices,sizeof(GLuint),data[i].indicecount,mwad);
    if (data[i].instancecount > 0)
    {
      fwrite(data[i].instances,sizeof(fn_mat4),data[i].instancecount,mwad);
      fwrite(data[i].ids,sizeof(fn_vec2),data[i].instancecount,mwad);
    }

    fclose(mwad);
  }
}

void th_importMWADs(th_GpuData* data,int count,const char* directory,const char* prefix)
{
  // #pragma omp parallel for
  for (int i = 0 ; i < count;i++)
  {
    uint8_t junk[16];
    char filename[1024];
    sprintf(filename,"%s%s%i.mwad",directory,prefix,i);
    FILE* mwad = th_fopen(filename,"rb");
    if (mwad == NULL)
    {
      printf("%s\n","CANNOT OPEN" );
    }
    fread(junk,sizeof(uint8_t),16,mwad);
    fread(&data[i].vertcount,sizeof(GLuint),1,mwad);
    fread(&data[i].indicecount,sizeof(GLuint),1,mwad);
    fread(&data[i].instancecount,sizeof(GLuint),1,mwad);
    data[i].verts = data[i].vertcount > 0 ? malloc(sizeof(th_Vertex)*data[i].vertcount) : NULL;
    data[i].indices = data[i].indicecount > 0 ? malloc(sizeof(GLuint)*data[i].indicecount) : NULL;
    if (data[i].instancecount > 0)
    {
      data[i].instances = malloc(sizeof(fn_mat4)*data[i].instancecount);
      data[i].ids = malloc(sizeof(fn_vec2)*data[i].instancecount);
    }

    fread(data[i].verts,sizeof(th_Vertex),data[i].vertcount,mwad);
    fread(data[i].indices,sizeof(GLuint),data[i].indicecount,mwad);
    if (data[i].instancecount > 0)
    {
      fread(data[i].instances,sizeof(fn_mat4),data[i].instancecount,mwad);
      fread(data[i].ids,sizeof(fn_vec2),data[i].instancecount,mwad);
    }
    fclose(mwad);
  }
}
