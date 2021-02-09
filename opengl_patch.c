/* opengl_patch.c -- reverse engineered shader generator translated for cg
 *
 * Copyright (C) 2021 Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <vitaGL.h>

#include "main.h"
#include "config.h"
#include "so_util.h"
#include "opengl_patch.h"

int (* CShaderProgram__AddFragmentShaderString)(void *this, char *string);
int (* CShaderProgram__AddVertexShaderString)(void *this, char *string);

#define FLAG_TEXTURE     0x01
#define FLAG_3D          0x02
#define FLAG_2D          0x04
#define FLAG_NORMALS     0x08
#define FLAG_EXTRA_LIGHT 0x10
#define FLAG_ALPHA_TEST  0x20

uint8_t CompilerRefCount = 0;

int CShaderProgram__CompileShaderWithFlags(void *this, unsigned int flags, int shaderType) {
  int shader;

  uint8_t is_fixed = 0;
  CompilerRefCount++;
  if (CompilerRefCount == 1) { // Program 1 - Vertex (This shader is used sometimes with GL_FIXED position attr)
    is_fixed = 1;
  }
  if (CompilerRefCount == 7) { // Program 4 - Vertex (This shader is used sometimes with GL_FIXED position attr)
    is_fixed = 1;
  }
  if (CompilerRefCount == 9) { // Program 5 - Vertex (This shader is used always with GL_FIXED position attr)
    is_fixed = 2;
  }
  
  char **shaderSource;

  // FRAGMENT SHADER
  if (shaderType != GL_VERTEX_SHADER) {
    CShaderProgram__AddFragmentShaderString(this, "float4 main(");
    if (flags & FLAG_TEXTURE) {
      CShaderProgram__AddFragmentShaderString(this, "uniform sampler2D SamplerDiffuse,");
      CShaderProgram__AddFragmentShaderString(this, "float2 Out_UV : TEXCOORD0,");
      CShaderProgram__AddFragmentShaderString(this, "float4 Out_Color : COLOR0");
      CShaderProgram__AddFragmentShaderString(this, "){");
      CShaderProgram__AddFragmentShaderString(this, "float4 gl_FragColor = tex2D ( SamplerDiffuse, Out_UV ) * Out_Color;");
    } else {
      CShaderProgram__AddFragmentShaderString(this, "float4 Out_Color : COLOR0");
      CShaderProgram__AddFragmentShaderString(this, "){");
      CShaderProgram__AddFragmentShaderString(this, "float4 gl_FragColor = Out_Color;");
    }
    if (flags & FLAG_ALPHA_TEST) {
      CShaderProgram__AddFragmentShaderString(this, "if ( gl_FragColor.a < 0.03125f ){ discard; }");
    }
    CShaderProgram__AddFragmentShaderString(this, "return gl_FragColor;");
    CShaderProgram__AddFragmentShaderString(this, "}");
    shader = glCreateShader(shaderType);
    shaderSource = (char **)(this + 92);

  // VERTEX SHADER
  } else {
    CShaderProgram__AddVertexShaderString(this, "void main(");
    CShaderProgram__AddVertexShaderString(this, "float3 vPos,");
    if (flags & FLAG_NORMALS)
      CShaderProgram__AddVertexShaderString(this, "float3 vNormal,");
    if (flags & FLAG_TEXTURE)
      CShaderProgram__AddVertexShaderString(this, "float2 vUV,");
    CShaderProgram__AddVertexShaderString(this, "float4 vColor,");
    if (is_fixed == 1)
      CShaderProgram__AddVertexShaderString(this, "uniform float is_fixed,");
    CShaderProgram__AddVertexShaderString(this, "uniform float4x4 matModelView,");
    CShaderProgram__AddVertexShaderString(this, "uniform float4x4 matProj,");
    if (flags & FLAG_NORMALS) {
      CShaderProgram__AddVertexShaderString(this, "uniform float3x3 matLastWorld33,");
      CShaderProgram__AddVertexShaderString(this, "uniform float4 vecMaterialAmbient,");
      CShaderProgram__AddVertexShaderString(this, "uniform float4 vecMaterialDiffuse,");
      CShaderProgram__AddVertexShaderString(this, "uniform float3 vecMainLightDirection,");
      CShaderProgram__AddVertexShaderString(this, "uniform float3 vecMainLightColorDiffuse,");
      CShaderProgram__AddVertexShaderString(this, "uniform float3 vecMainLightColorAmbient,");
      if (flags & FLAG_EXTRA_LIGHT) {
        CShaderProgram__AddVertexShaderString(this, "uniform float3x3 matView33,");
        CShaderProgram__AddVertexShaderString(this, "uniform float3 vecExtraLightPosition,");
        CShaderProgram__AddVertexShaderString(this, "uniform float3 vecExtraLightColorDiffuse,");
      }
    }
    if (flags & FLAG_TEXTURE)
      CShaderProgram__AddVertexShaderString(this, "float2 out Out_UV : TEXCOORD0,");
    CShaderProgram__AddVertexShaderString(this, "float4 out Out_Color : COLOR0,");
    CShaderProgram__AddVertexShaderString(this, "float4 out gl_Position : POSITION");
    CShaderProgram__AddVertexShaderString(this, "){");
    if (is_fixed == 1)
      CShaderProgram__AddVertexShaderString(this, "if (is_fixed > 0.0f)");
    if (is_fixed)
      CShaderProgram__AddVertexShaderString(this, "vPos = floatToIntBits(vPos) / 65536.0f;");
    if (flags & FLAG_3D) {
      CShaderProgram__AddVertexShaderString(this, "float4 vecPos = float4 ( vPos, 1.0f );");
      CShaderProgram__AddVertexShaderString(this, "gl_Position = mul ( mul ( vecPos, matModelView ), matProj );");
    } else if (flags & FLAG_2D) {
      CShaderProgram__AddVertexShaderString(this, "float4 vecPos = float4 ( vPos.xy, 0.0f, 1.0f );");
      CShaderProgram__AddVertexShaderString(this, "gl_Position = mul ( mul ( vecPos, matModelView ), matProj );");
    }
    if (flags & FLAG_NORMALS) {
      CShaderProgram__AddVertexShaderString(this, "float3 vNormalized = normalize ( vNormal );");
      CShaderProgram__AddVertexShaderString(this, "float3 vecTransformedNormal = mul ( vNormalized, matLastWorld33 );");
      CShaderProgram__AddVertexShaderString(this, "float fIntensity = dot ( vecTransformedNormal, vecMainLightDirection );");
      CShaderProgram__AddVertexShaderString(this, "if ( fIntensity < 0.0f ) fIntensity = 0.0f;");
      CShaderProgram__AddVertexShaderString(this, "float4 vecIntensity = float4 ( fIntensity, fIntensity, fIntensity, 1.0f );");
      CShaderProgram__AddVertexShaderString(this, "float4 vecAmbient = 0.4f * float4 ( vecMainLightColorAmbient.rgb, 0.0f ) * float4 ( vecMaterialAmbient.rgb, 0.0f );");
      CShaderProgram__AddVertexShaderString(this, "float4 vecLighting = float4 ( vecMainLightColorDiffuse.rgb, 1.0f ) * vecMaterialDiffuse * vecIntensity + vecAmbient;");
      CShaderProgram__AddVertexShaderString(this, "float4 vecTotalLighting = vecLighting;");
      if (flags & FLAG_EXTRA_LIGHT) {
        CShaderProgram__AddVertexShaderString(this, "float3 vecTransformedViewNormal = mul ( vNormalized, matView33 );");
        CShaderProgram__AddVertexShaderString(this, "float4 vecView = mul ( float4 ( vPos, 1.0f ), matModelView );");
        CShaderProgram__AddVertexShaderString(this, "float3 vecViewToLight = float3 ( vecView.x - vecExtraLightPosition.x, vecView.y - vecExtraLightPosition.y, vecView.z - vecExtraLightPosition.z );");
        CShaderProgram__AddVertexShaderString(this, "float fDistance = length ( vecViewToLight );");
        CShaderProgram__AddVertexShaderString(this, "fIntensity = 1.0f - fDistance * fDistance * 0.002125f;");
        CShaderProgram__AddVertexShaderString(this, "if ( fIntensity < 0.0f ) fIntensity = 0.0f;");
        CShaderProgram__AddVertexShaderString(this, "float3 vecVertexToLight = float3 ( vecView.x - vecExtraLightPosition.x, vecView.y - vecExtraLightPosition.y, vecView.z - vecExtraLightPosition.z );");
        CShaderProgram__AddVertexShaderString(this, "float3 vecVertexToLightNormalized = normalize ( vecVertexToLight );");
        CShaderProgram__AddVertexShaderString(this, "float fDotIntensity = dot ( vecTransformedViewNormal, -vecVertexToLightNormalized );");
        CShaderProgram__AddVertexShaderString(this, "if ( fDotIntensity < 0.0f ) fDotIntensity = 0.0f;");
        CShaderProgram__AddVertexShaderString(this, "fIntensity *= fDotIntensity;");
        CShaderProgram__AddVertexShaderString(this, "vecTotalLighting += float4 ( fIntensity * vecExtraLightColorDiffuse.r, fIntensity * vecExtraLightColorDiffuse.g, fIntensity * vecExtraLightColorDiffuse.b, 0.0f );");
      }
      CShaderProgram__AddVertexShaderString(this, "if ( vecTotalLighting.r > 1.0f ) vecTotalLighting.r = 1.0f;");
      CShaderProgram__AddVertexShaderString(this, "if ( vecTotalLighting.g > 1.0f ) vecTotalLighting.g = 1.0f;");
      CShaderProgram__AddVertexShaderString(this, "if ( vecTotalLighting.b > 1.0f ) vecTotalLighting.b = 1.0f;");
      CShaderProgram__AddVertexShaderString(this, "Out_Color = vecTotalLighting;");
    }else {
      CShaderProgram__AddVertexShaderString(this, "Out_Color = vColor;");
    }
    if (flags & FLAG_TEXTURE)
      CShaderProgram__AddVertexShaderString(this, "Out_UV = float2 ( vUV.x * 0.000488f, vUV.y * 0.000488f );");
    CShaderProgram__AddVertexShaderString(this, "}");

    shader = glCreateShader(GL_VERTEX_SHADER);
    shaderSource = (char **)(this + 88);
  }

  glShaderSource(shader, 1, (const char **)shaderSource, NULL);
  glCompileShader(shader);

  return shader;
}

void patch_opengl(void) {
  CShaderProgram__AddFragmentShaderString = (void *)so_find_addr("_ZN14CShaderProgram23AddFragmentShaderStringEPKc");
  CShaderProgram__AddVertexShaderString = (void *)so_find_addr("_ZN14CShaderProgram21AddVertexShaderStringEPKc");
  hook_thumb(so_find_addr("_ZN14CShaderProgram22CompileShaderWithFlagsEji"), (uintptr_t)CShaderProgram__CompileShaderWithFlags);
}
