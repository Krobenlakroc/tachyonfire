#include "th_brdf.h"

#define PI 3.14159265359

#include <stdio.h>
#include <stdlib.h>
#include "../th_fopen.h"

float RadicalInverse_VdC(GLuint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return (float)bits * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
fn_vec2 Hammersley(GLuint i, GLuint N)
{
    return fn_createVec2((float)i/(float)N, RadicalInverse_VdC(i));
}

fn_vec3 ImportanceSampleGGX(fn_vec2 Xi, fn_vec3 N, float roughness)
{
    float a = roughness*roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrtf((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrtf(1.0 - cosTheta*cosTheta);

    // from spherical coordinates to cartesian coordinates
    fn_vec3 H;
    H.x = cosf(phi) * sinTheta;
    H.y = sinf(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    fn_vec3 up        = fabs(N.z) < 0.999 ? fn_createVec3(0.0, 0.0, 1.0) : fn_createVec3(1.0, 0.0, 0.0);
    fn_vec3 tangent   = fn_normalizeVec3(fn_cross(up, N));
    fn_vec3 bitangent = fn_cross(N, tangent);

    fn_vec3 sampleVec = fn_addVec3(fn_addVec3(fn_multVec3s(tangent , H.x),fn_multVec3s(bitangent , H.y)),fn_multVec3s(N , H.z));
    return fn_normalizeVec3(sampleVec);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(fn_vec3 N, fn_vec3 V, fn_vec3 L, float roughness)
{
    float NdotV = fn_max(fn_dot(N, V), 0.0);
    float NdotL = fn_max(fn_dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

fn_vec2 IntegrateBRDF(float NdotV, float roughness)
{
    fn_vec3 V;
    V.x = sqrtf(1.0 - NdotV*NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    fn_vec3 N = fn_createVec3(0.0, 0.0, 1.0);

    const GLuint SAMPLE_COUNT = 1024;
    for(GLuint i = 0; i < SAMPLE_COUNT; ++i)
    {
        fn_vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        fn_vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        fn_vec3 L  = fn_normalizeVec3(fn_subVec3(fn_multVec3s(H,2.0 * fn_dot(V, H)),V));

        float NdotL = fn_max(L.z, 0.0);
        float NdotH = fn_max(H.z, 0.0);
        float VdotH = fn_max(fn_dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = powf(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= (float)SAMPLE_COUNT;
    B /= (float)SAMPLE_COUNT;
    return fn_createVec2(A, B);
}
void th_saveLUT(GLfloat* data,int dimension,const char* filename)
{
    FILE* fp = th_fopen(filename,"w");

    for (int j = 0 ; j < dimension*dimension;j++)
    {
        fprintf(fp, "%f %f ",data[j*2 + 0],data[j*2 + 1] );
    }

    fclose(fp);
}

void th_loadLUT(int dimension,const char* filename,GLuint* brdfLUTTexture)
{

    GLfloat* data = malloc(sizeof(GLfloat)*2*512*512);

    FILE* fp = th_fopen(filename,"r");

    for (int j = 0 ; j < dimension*dimension;j++)
    {
        fscanf(fp, "%f %f ",&data[j*2 + 0],&data[j*2 + 1] );
    }

    fclose(fp);
    glGenTextures(1, brdfLUTTexture);
    glBindTexture(GL_TEXTURE_2D, *brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void th_generateBRDFLUT(GLuint* brdfLUTTexture)
{
    GLfloat* data = malloc(sizeof(GLfloat)*2*512*512);
    for (int i = 0 ; i < 512*512;i++)
    {
        float x = i % 512;
        float y = (i - x)/512;
        x = x/512.0;
        y = y/512.0;
        fn_vec2 brdf =  IntegrateBRDF(fn_max(x,0.001),y);
        data[i*2 + 0] = brdf.x;
        data[i*2 + 1] = brdf.y;
    }
    th_saveLUT(data,512,"th1/brdflut.float");

    glGenTextures(1, brdfLUTTexture);
    glBindTexture(GL_TEXTURE_2D, *brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

}
