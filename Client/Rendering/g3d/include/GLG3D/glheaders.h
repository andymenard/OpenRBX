/**
 @file G3D/glheaders.h

 #includes the OpenGL headers

 @maintainer Morgan McGuire, matrix@graphics3d.com

 @created 2002-08-07
 @edited  2005-04-10

 Copyright 2002-2006, Morgan McGuire.
 All rights reserved.
*/

#ifndef G3D_GLHEADERS_H
#define G3D_GLHEADERS_H

#include "G3D/platform.h"
#if defined(G3D_WIN32) && ! defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#endif

#include "../GL/gl.h"
#include "../GL/glext.h"

#ifdef G3D_WIN32
    #include "../GL/wglext.h"
#endif

#if defined(G3D_GL4ES)
// OpenGL calls go to gl4es, which translates them to OpenGL ES on the current EGL context.
#include <EGL/egl.h>
extern "C" void* gl4es_GetProcAddress(const char* name);
#elif defined(G3D_EGL)
    #include <EGL/egl.h>
#elif defined(G3D_LINUX)
    typedef struct __GLXcontextRec *GLXContext;
    extern "C" GLXContext glXGetCurrentContext(void);
    extern "C" void (*glXGetProcAddressARB(const GLubyte *procName))(void);
#endif

#include "../GL/glext.h"

#ifdef G3D_OSX
#include <OpenGL/glu.h>
#include <OpenGL/OpenGL.h>
#else
#include <GL/glu.h>
#endif

// OpenGL extensions


// OpenGL 1.2.1 and later define these as part of the static link; don't 
// define them.
#ifndef G3D_GL_ARB_multitexture_static
extern PFNGLMULTITEXCOORD2FARBPROC         glMultiTexCoord2fARB;

extern PFNGLMULTITEXCOORD1FARBPROC         glMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD1DARBPROC         glMultiTexCoord1dARB;

extern PFNGLMULTITEXCOORD2FVARBPROC        glMultiTexCoord2fvARB;
extern PFNGLMULTITEXCOORD2DVARBPROC        glMultiTexCoord2dvARB;

extern PFNGLMULTITEXCOORD3FVARBPROC        glMultiTexCoord3fvARB;
extern PFNGLMULTITEXCOORD3DVARBPROC        glMultiTexCoord3dvARB;

extern PFNGLMULTITEXCOORD4FVARBPROC        glMultiTexCoord4fvARB;
extern PFNGLMULTITEXCOORD4DVARBPROC        glMultiTexCoord4dvARB;
extern PFNGLACTIVETEXTUREARBPROC           glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC     glClientActiveTextureARB;

#endif


extern PFNGLBLENDEQUATIONEXTPROC           glBlendEquationEXT;

extern PFNGLVERTEXARRAYRANGENVPROC         glVertexArrayRangeNV;
extern PFNGLFLUSHVERTEXARRAYRANGENVPROC    glFlushVertexArrayRangeNV;

extern PFNGLCOMPRESSEDTEXIMAGE2DARBPROC    glCompressedTexImage2DARB;
extern PFNGLGETCOMPRESSEDTEXIMAGEARBPROC   glGetCompressedTexImageARB;

extern PFNGLVERTEXATTRIBPOINTERARBPROC     glVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;

extern PFNGLPOINTPARAMETERFARBPROC         glPointParameterfARB;
extern PFNGLPOINTPARAMETERFVARBPROC        glPointParameterfvARB;

#ifdef G3D_WIN32
    typedef BOOL (APIENTRY * PFNWGLGLSWAPINTERVALEXTPROC) (GLint interval);
    typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
    extern PFNWGLGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT;
    extern PFNWGLCHOOSEPIXELFORMATARBPROC      wglChoosePixelFormatARB;
    extern PFNWGLALLOCATEMEMORYNVPROC          wglAllocateMemoryNV;
    extern PFNWGLFREEMEMORYNVPROC              wglFreeMemoryNV;
#endif
extern PFNGLVERTEXARRAYRANGENVPROC         glVertexArrayRangeNV;

extern PFNGLMULTIDRAWARRAYSEXTPROC glMultiDrawArraysEXT;
extern PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElementsEXT;

  
#ifdef GL_NV_fence
extern PFNGLGENFENCESNVPROC				   glGenFencesNV;
extern PFNGLDELETEFENCESNVPROC			   glDeleteFencesNV;
extern PFNGLSETFENCENVPROC				   glSetFenceNV;
extern PFNGLFINISHFENCENVPROC			   glFinishFenceNV;
#endif


extern PFNGLGENPROGRAMSNVPROC              glGenProgramsNV;
extern PFNGLDELETEPROGRAMSNVPROC           glDeleteProgramsNV;
extern PFNGLBINDPROGRAMNVPROC              glBindProgramNV;
extern PFNGLLOADPROGRAMNVPROC              glLoadProgramNV;
extern PFNGLTRACKMATRIXNVPROC              glTrackMatrixNV;
extern PFNGLPROGRAMPARAMETER4FVNVPROC      glProgramParameter4fvNV;
extern PFNGLGETPROGRAMPARAMETERFVNVPROC    glGetProgramParameterfvNV;
extern PFNGLGETPROGRAMPARAMETERDVNVPROC    glGetProgramParameterdvNV;

extern PFNGLGENPROGRAMSARBPROC                     glGenProgramsARB;
extern PFNGLBINDPROGRAMARBPROC                     glBindProgramARB;
extern PFNGLDELETEPROGRAMSARBPROC                  glDeleteProgramsARB;
extern PFNGLPROGRAMSTRINGARBPROC                   glProgramStringARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC           glProgramEnvParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC         glProgramLocalParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FVARBPROC        glProgramLocalParameter4fvARB;
extern PFNGLPROGRAMENVPARAMETER4DVARBPROC          glProgramEnvParameter4dvARB;
extern PFNGLPROGRAMLOCALPARAMETER4DVARBPROC        glProgramLocalParameter4dvARB;

extern PFNGLCOMBINERPARAMETERFVNVPROC               glCombinerParameterfvNV;
extern PFNGLCOMBINERPARAMETERFNVPROC                glCombinerParameterfNV;
extern PFNGLCOMBINERPARAMETERIVNVPROC               glCombinerParameterivNV;
extern PFNGLCOMBINERPARAMETERINVPROC                glCombinerParameteriNV;
extern PFNGLCOMBINERINPUTNVPROC                     glCombinerInputNV;
extern PFNGLCOMBINEROUTPUTNVPROC                    glCombinerOutputNV;
extern PFNGLFINALCOMBINERINPUTNVPROC                glFinalCombinerInputNV;
extern PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC       glGetCombinerInputParameterfvNV;
extern PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC       glGetCombinerInputParameterivNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC      glGetCombinerOutputParameterfvNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC      glGetCombinerOutputParameterivNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC  glGetFinalCombinerInputParameterfvNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC  glGetFinalCombinerInputParameterivNV;
extern PFNGLCOMBINERSTAGEPARAMETERFVNVPROC          glCombinerStageParameterfvNV;
extern PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC       glGetCombinerStageParameterfvNV;

extern PFNGLACTIVESTENCILFACEEXTPROC                glActiveStencilFaceEXT;

#ifdef G3D_WIN32
    extern PFNWGLALLOCATEMEMORYNVPROC               wglAllocateMemoryNV;
    extern PFNWGLFREEMEMORYNVPROC                   wglFreeMemoryNV;
#endif

extern PFNGLBINDBUFFERARBPROC glBindBufferARB;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC glGenBuffersARB;
extern PFNGLISBUFFERARBPROC glIsBufferARB;
extern PFNGLBUFFERDATAARBPROC glBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC glBufferSubDataARB;
extern PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB;
extern PFNGLMAPBUFFERARBPROC glMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC glGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC glGetBufferPointervARB;

extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;


extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETHANDLEARBPROC glGetHandleARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
extern PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
extern PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
extern PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
extern PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUNIFORM2IARBPROC glUniform2iARB;
extern PFNGLUNIFORM3IARBPROC glUniform3iARB;
extern PFNGLUNIFORM4IARBPROC glUniform4iARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
extern PFNGLGETATTRIBLOCATIONARB glGetAttribLocationARB;

extern PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT;

#ifdef G3D_WIN32

typedef BOOL (*PFNWGLENABLEGENLOCKI3D) (HDC hDCGL);
extern PFNWGLENABLEGENLOCKI3D wglEnableGenlockI3D;

#endif

// Occlusion query
extern PFNGLBEGINQUERYARBPROC glBeginQueryARB;
extern PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;
extern PFNGLENDQUERYARB glEndQueryARB;
extern PFNGLGENQUERIESARBPROC glGenQueriesARB;
extern PFNGLGETQUERYIVARBPROC glGetQueryivARB;
extern PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;
extern PFNGLISQUERYARBPROC glIsQueryARB;

extern PFNGLDELETEFRAGMENTSHADERATIPROC glDeleteFragmentShaderATI;
extern PFNGLGENFRAGMENTSHADERSATIPROC   glGenFragmentShadersATI;
extern PFNGLBINDFRAGMENTSHADERATIPROC   glBindFragmentShaderATI;
extern PFNGLBEGINFRAGMENTSHADERATIPROC  glBeginFragmentShaderATI;
extern PFNGLENDFRAGMENTSHADERATIPROC    glEndFragmentShaderATI;
extern PFNGLPASSTEXCOORDATIPROC         glPassTexCoordATI;
extern PFNGLSAMPLEMAPATIPROC            glSampleMapATI;
extern PFNGLSETFRAGMENTSHADERCONSTANTATIPROC glSetFragmentShaderConstantATI;
extern PFNGLCOLORFRAGMENTOP1ATIPROC glColorFragmentOp1ATI;
extern PPFNGLCOLORFRAGMENTOP2ATIPROC glColorFragmentOp2ATI;
extern PFNGLCOLORFRAGMENTOP3ATIPROC glColorFragmentOp3ATI;
extern PFNGLALPHAFRAGMENTOP1ATIPROC glAlphaFragmentOp1ATI;
extern PFNGLALPHAFRAGMENTOP2ATIPROC glAlphaFragmentOp2ATI;
extern PFNGLALPHAFRAGMENTOP3ATIPROC glAlphaFragmentOp3ATI;

// Frame buffer object
extern PFNGLISRENDERBUFFEREXTPROC glIsRenderBufferEXT;
extern PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
extern PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
extern PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
extern PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glGetRenderbufferParameterivEXT;
extern PFNGLISFRAMEBUFFEREXTPROC glIsFramebufferEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
extern PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
extern PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glFramebufferTexture1DEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
extern PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glFramebufferTexture3DEXT;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glGetFramebufferAttachmentParameterivEXT;
extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;

// ATI separate stencil
extern PFNGLSTENCILFUNCSEPARATEATIPROC glStencilFuncSeparateATI;
extern PFNGLSTENCILOPSEPARATEATIPROC glStencilOpSeparateATI;

#if defined(G3D_OSX)
namespace G3D
{
    /* The OpenGL implementation on OS X has 
	   no built-in way of getting an extension
	   proc addresses.  The function that does
	   this is in the GLG3D library.  */
	void* NSGLGetProcAddress(const char *name);
}
#endif

#ifndef GL_CLAMP_TO_BORDER_SGIS
#define GL_CLAMP_TO_BORDER_SGIS           0x812D
#endif

#ifndef  GL_TEXTURE_BINDING_3D
#define  GL_TEXTURE_BINDING_3D   0x806A 
#endif


extern PFNGLDRAWBUFFERSARBPROC glDrawBuffersARB;

#ifdef G3D_WIN32

extern PFNWGLCREATEPBUFFEREXTPROC wglCreatePbufferEXT;
extern PFNWGLDESTROYPBUFFEREXTPROC wglDestroyPbufferEXT;

#endif


extern PFNGLDEPTHBOUNDSEXTPROC glDepthBoundsEXT;

#endif
