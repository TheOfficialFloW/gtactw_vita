/* main.c -- Grand Theft Auto: Chinatown Wars .so loader
 *
 * Copyright (C) 2021 Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <kubridge.h>
#include <vitashark.h>
#include <vitaGL.h>

#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <math.h>
#include <math_neon.h>

#include <errno.h>
#include <ctype.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "main.h"
#include "config.h"
#include "so_util.h"
#include "jni_patch.h"
#include "openal_patch.h"
#include "opengl_patch.h"

int _newlib_heap_size_user = MEMORY_MB * 1024 * 1024;

SceTouchPanelInfo panelInfoFront, panelInfoBack;

int debugPrintf(char *text, ...) {
  va_list list;
  static char string[0x1000];

  va_start(list, text);
  vsprintf(string, text, list);
  va_end(list);

  SceUID fd = sceIoOpen("ux0:data/gtactw_log.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, string, strlen(string));
    sceIoClose(fd);
  }

  return 0;
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
  va_list list;
  static char string[0x1000];

  va_start(list, fmt);
  vsprintf(string, fmt, list);
  va_end(list);

  debugPrintf("%s: %s\n", tag, string);

  return 0;
}

int ret0(void) {
  return 0;
}

int ret1(void) {
  return 1;
}

int mkdir(const char *pathname, mode_t mode) {
  if (sceIoMkdir(pathname, mode) < 0)
    return -1;
  return 0;
}

int OS_ScreenGetHeight(void) {
  return SCREEN_H;
}

int OS_ScreenGetWidth(void) {
  return SCREEN_W;
}

int ProcessEvents(void) {
  return 0; // 1 is exit!
}

// only used for NVEventAppMain
int pthread_create_fake(int r0, int r1, int r2, void *arg) {
  int (* func)() = *(void **)(arg + 4);
  return func();
}

int pthread_mutex_init_fake(SceKernelLwMutexWork **work) {
  *work = (SceKernelLwMutexWork *)memalign(8, sizeof(SceKernelLwMutexWork));
  if (sceKernelCreateLwMutex(*work, "mutex", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL) < 0)
    return -1;
  return 0;
}

int pthread_mutex_destroy_fake(SceKernelLwMutexWork **work) {
  if (sceKernelDeleteLwMutex(*work) < 0)
    return -1;
  free(*work);
  return 0;
}

int pthread_mutex_lock_fake(SceKernelLwMutexWork **work) {
  if (!*work)
    pthread_mutex_init_fake(work);
  if (sceKernelLockLwMutex(*work, 1, NULL) < 0)
    return -1;
  return 0;
}

int pthread_mutex_unlock_fake(SceKernelLwMutexWork **work) {
  if (sceKernelUnlockLwMutex(*work, 1) < 0)
    return -1;
  return 0;
}

int sem_init_fake(int *uid) {
  *uid = sceKernelCreateSema("sema", 0, 0, 0x7fffffff, NULL);
  if (*uid < 0)
    return -1;
  return 0;
}

int sem_post_fake(int *uid) {
  if (sceKernelSignalSema(*uid, 1) < 0)
    return -1;
  return 0;
}

int sem_wait_fake(int *uid) {
  if (sceKernelWaitSema(*uid, 1, NULL) < 0)
    return -1;
  return 0;
}

int sem_trywait_fake(int *uid) {
  SceUInt timeout = 0;
  if (sceKernelWaitSema(*uid, 1, &timeout) < 0)
    return -1;
  return 0;
}

int sem_destroy_fake(int *uid) {
  if (sceKernelDeleteSema(*uid) < 0)
    return -1;
  return 0;
}

int thread_stub(SceSize args, uintptr_t *argp) {
  int (* func)(void *arg) = (void *)argp[0];
  void *arg = (void *)argp[1];
  char *out = (char *)argp[2];
  out[0x41] = 1; // running
  func(arg);
  return sceKernelExitDeleteThread(0);
}

void *OS_ThreadLaunch(int (* func)(), void *arg, int r2, char *name, int r4, int priority) {
  int vita_priority;

  switch (priority) {
    case 0:
      vita_priority = 0x40;
      break;
    case 1:
      vita_priority = 0x10000100 - 31;
      break;
    case 2:
      vita_priority = 0x10000100 - 15;
      break;
    case 3:
      vita_priority = 0x10000100;
      break;
    default:
      vita_priority = 0x10000100;
      break;
  }

  SceUID thid = sceKernelCreateThread(name, (SceKernelThreadEntry)thread_stub, vita_priority, 128 * 1024, 0, 0, NULL);
  if (thid >= 0) {
    char *out = malloc(0x48);
    *(int *)(out + 0x24) = thid;

    uintptr_t args[3];
    args[0] = (uintptr_t)func;
    args[1] = (uintptr_t)arg;
    args[2] = (uintptr_t)out;
    sceKernelStartThread(thid, sizeof(args), args);

    return out;
  }

  return NULL;
}

GLfloat is_fixed;
GLint is_fixed_unif_0;
GLint is_fixed_unif_4;

void glVertexAttribPointerHook(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
  if (index == 0) {
    if (type == GL_FIXED) {
      is_fixed = 1.0f;
      type = GL_FLOAT;
    } else {
      is_fixed = 0.0f;
    }
  }
  glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

GLuint cur_prog;
void glDrawArraysHook(GLenum mode, GLint first, GLsizei count) {
  switch (cur_prog) {
  case 0:
    glUniform1f(is_fixed_unif_0, is_fixed);
    break;
  case 4:
    glUniform1f(is_fixed_unif_4, is_fixed);
    break;
  default:
    break;
  }
  glDrawArrays(mode, first, count);
}

void glLinkProgramHook(GLuint program) {
  glLinkProgram(program);
  switch (program) {
  case 0:
    is_fixed_unif_0 = glGetUniformLocation(program, "is_fixed");
    break;
  case 4:
    is_fixed_unif_4 = glGetUniformLocation(program, "is_fixed");
    break;
  default:
    break;
  }
}

void glUseProgramHook(GLuint program) {
  glUseProgram(program);
  cur_prog = program;
}

void OS_ThreadWait(void *thread) {
  if (thread)
    sceKernelWaitThreadEnd(*(int *)(thread + 0x24), NULL, NULL);
}

extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;

void patch_game(void) {
  hook_thumb(so_find_addr("__cxa_guard_acquire"), (uintptr_t)&__cxa_guard_acquire);
  hook_thumb(so_find_addr("__cxa_guard_release"), (uintptr_t)&__cxa_guard_release);

  hook_thumb(so_find_addr("_Z24NVThreadGetCurrentJNIEnvv"), (uintptr_t)NVThreadGetCurrentJNIEnv);

  // do not use pthread
  hook_thumb(so_find_addr("_Z15OS_ThreadLaunchPFjPvES_jPKci16OSThreadPriority"), (uintptr_t)OS_ThreadLaunch);
  hook_thumb(so_find_addr("_Z13OS_ThreadWaitPv"), (uintptr_t)OS_ThreadWait);

  hook_thumb(so_find_addr("_Z17OS_ScreenGetWidthv"), (uintptr_t)OS_ScreenGetWidth);
  hook_thumb(so_find_addr("_Z18OS_ScreenGetHeightv"), (uintptr_t)OS_ScreenGetHeight);

  // TODO: set deviceChip, definedDevice
  hook_thumb(so_find_addr("_Z20AND_SystemInitializev"), (uintptr_t)ret0);

  // TODO: implement touch here
  hook_thumb(so_find_addr("_Z13ProcessEventsb"), (uintptr_t)ProcessEvents);
}

extern void *__cxa_atexit;
extern void *__cxa_finalize;
extern void *__stack_chk_fail;

int __signbit(double d) {
  return signbit(d);
}

int __isfinite(double d) {
  return isfinite(d);
}

void *sceClibMemclr(void *dst, SceSize len) {
  return sceClibMemset(dst, 0, len);
}

void *sceClibMemset2(void *dst, SceSize len, int ch) {
  return sceClibMemset(dst, ch, len);
}

static int __stack_chk_guard_fake = 0x42424242;

static FILE *stderr_fake;
static FILE __sF_fake[0x100][3];

static DynLibFunction dynlib_functions[] = {
  { "__aeabi_memclr", (uintptr_t)&sceClibMemclr },
  { "__aeabi_memclr4", (uintptr_t)&sceClibMemclr },
  { "__aeabi_memclr8", (uintptr_t)&sceClibMemclr },
  { "__aeabi_memcpy", (uintptr_t)&sceClibMemcpy },
  { "__aeabi_memcpy4", (uintptr_t)&sceClibMemcpy },
  { "__aeabi_memcpy8", (uintptr_t)&sceClibMemcpy },
  { "__aeabi_memmove", (uintptr_t)&sceClibMemmove },
  { "__aeabi_memmove4", (uintptr_t)&sceClibMemmove },
  { "__aeabi_memmove8", (uintptr_t)&sceClibMemmove },
  { "__aeabi_memset", (uintptr_t)&sceClibMemset2 },
  { "__aeabi_memset4", (uintptr_t)&sceClibMemset2 },
  { "__aeabi_memset8", (uintptr_t)&sceClibMemset2 },

  // { "__assert2", (uintptr_t)&__assert2 },
  { "__android_log_print", (uintptr_t)&__android_log_print },
  { "__cxa_atexit", (uintptr_t)&__cxa_atexit },
  { "__cxa_finalize", (uintptr_t)&__cxa_finalize },
  { "__errno", (uintptr_t)&__errno },
  { "__isfinite", (uintptr_t)&__isfinite },
  { "__sF", (uintptr_t)&__sF_fake },
  { "__signbit", (uintptr_t)&__signbit },
  { "__stack_chk_fail", (uintptr_t)&__stack_chk_fail },
  { "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },

  { "AAssetManager_fromJava", (uintptr_t)&ret0 },
  { "AAssetManager_open", (uintptr_t)&ret0 },
  { "AAsset_close", (uintptr_t)&ret0 },
  { "AAsset_getLength", (uintptr_t)&ret0 },
  { "AAsset_getRemainingLength", (uintptr_t)&ret0 },
  { "AAsset_read", (uintptr_t)&ret0 },
  { "AAsset_seek", (uintptr_t)&ret0 },

  { "pthread_create", (uintptr_t)&pthread_create_fake },
  { "pthread_cond_init", (uintptr_t)&ret0 },
  { "pthread_getspecific", (uintptr_t)&ret0 },
  { "pthread_key_create", (uintptr_t)&ret0 },
  { "pthread_mutexattr_init", (uintptr_t)&ret0 },
  { "pthread_mutexattr_settype", (uintptr_t)&ret0 },
  { "pthread_mutexattr_destroy", (uintptr_t)&ret0 },
  { "pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake },
  { "pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake },
  { "pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake },
  { "pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake },
  { "pthread_setspecific", (uintptr_t)&ret0 },

  { "sem_destroy", (uintptr_t)&sem_destroy_fake },
  // { "sem_getvalue", (uintptr_t)&sem_getvalue },
  { "sem_init", (uintptr_t)&sem_init_fake },
  { "sem_post", (uintptr_t)&sem_post_fake },
  { "sem_trywait", (uintptr_t)&sem_trywait_fake },
  { "sem_wait", (uintptr_t)&sem_wait_fake },

  { "sigaction", (uintptr_t)&ret0 },
  { "sigemptyset", (uintptr_t)&ret0 },

  // { "accept", (uintptr_t)&accept },
  // { "bind", (uintptr_t)&bind },
  // { "connect", (uintptr_t)&connect },
  // { "gethostbyaddr", (uintptr_t)&gethostbyaddr },
  // { "gethostbyname", (uintptr_t)&gethostbyname },
  // { "getsockname", (uintptr_t)&getsockname },
  // { "getsockopt", (uintptr_t)&getsockopt },
  // { "inet_aton", (uintptr_t)&inet_aton },
  // { "inet_ntoa", (uintptr_t)&inet_ntoa },
  // { "ioctl", (uintptr_t)&ioctl },
  // { "listen", (uintptr_t)&listen },
  // { "recvmsg", (uintptr_t)&recvmsg },
  // { "sendmsg", (uintptr_t)&sendmsg },
  // { "socket", (uintptr_t)&socket },
  // { "setsockopt", (uintptr_t)&setsockopt },
  // { "shutdown", (uintptr_t)&shutdown },

  { "acosf", (uintptr_t)&acosf },
  { "asinf", (uintptr_t)&asinf },
  { "atan", (uintptr_t)&atan },
  { "atan2f", (uintptr_t)&atan2f },
  { "atanf", (uintptr_t)&atanf },
  { "cos", (uintptr_t)&cos },
  { "cosf", (uintptr_t)&cosf },
  { "exp", (uintptr_t)&exp },
  { "expf", (uintptr_t)&expf },
  { "exp2", (uintptr_t)&exp2 },
  { "floor", (uintptr_t)&floor },
  { "floorf", (uintptr_t)&floorf },
  { "log", (uintptr_t)&log },
  { "log10f", (uintptr_t)&log10f },
  { "logf", (uintptr_t)&logf },
  { "pow", (uintptr_t)&pow },
  { "powf", (uintptr_t)&powf },
  { "sin", (uintptr_t)&sin },
  { "sinf", (uintptr_t)&sinf },
  { "tan", (uintptr_t)&tan },

  { "atof", (uintptr_t)&atof },
  { "atoi", (uintptr_t)&atoi },

  { "isspace", (uintptr_t)&isspace },

  { "calloc", (uintptr_t)&calloc },
  { "free", (uintptr_t)&free },
  { "malloc", (uintptr_t)&malloc },
  { "realloc", (uintptr_t)&realloc },

  // { "clock_gettime", (uintptr_t)&clock_gettime },
  { "gettimeofday", (uintptr_t)&gettimeofday },
  { "gmtime", (uintptr_t)&gmtime },
  { "localtime", (uintptr_t)&localtime },
  { "localtime_r", (uintptr_t)&localtime_r },
  { "time", (uintptr_t)&time },

  { "eglGetDisplay", (uintptr_t)&ret0 },
  { "eglGetProcAddress", (uintptr_t)&ret0 },
  { "eglQueryString", (uintptr_t)&ret0 },

  { "abort", (uintptr_t)&abort },

  { "fclose", (uintptr_t)&fclose },
  { "feof", (uintptr_t)&feof },
  // { "fflush", (uintptr_t)&fflush },
  // { "fgetc", (uintptr_t)&fgetc },
  // { "fgets", (uintptr_t)&fgets },
  { "fopen", (uintptr_t)&fopen },
  // { "fprintf", (uintptr_t)&fprintf },
  // { "fputc", (uintptr_t)&fputc },
  // { "fputs", (uintptr_t)&fputs },
  // { "fputwc", (uintptr_t)&fputwc },
  { "fread", (uintptr_t)&fread },
  { "fseek", (uintptr_t)&fseek },
  { "ftell", (uintptr_t)&ftell },
  { "fwrite", (uintptr_t)&fwrite },

  { "getenv", (uintptr_t)&getenv },
  // { "gettid", (uintptr_t)&gettid },

  { "glActiveTexture", (uintptr_t)&glActiveTexture },
  { "glAttachShader", (uintptr_t)&glAttachShader },
  { "glBindAttribLocation", (uintptr_t)&glBindAttribLocation },
  { "glBindBuffer", (uintptr_t)&glBindBuffer },
  { "glBindFramebuffer", (uintptr_t)&glBindFramebuffer },
  { "glBindRenderbuffer", (uintptr_t)&ret0 },
  { "glBindTexture", (uintptr_t)&glBindTexture },
  { "glBlendEquation", (uintptr_t)&glBlendEquation },
  { "glBlendFunc", (uintptr_t)&glBlendFunc },
  { "glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus },
  { "glClear", (uintptr_t)&glClear },
  { "glClearColor", (uintptr_t)&glClearColor },
  { "glColorMask", (uintptr_t)&glColorMask },
  { "glCompileShader", (uintptr_t)&glCompileShader },
  { "glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D },
  { "glCreateProgram", (uintptr_t)&glCreateProgram },
  { "glCreateShader", (uintptr_t)&glCreateShader },
  { "glCullFace", (uintptr_t)&glCullFace },
  { "glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers },
  { "glDeleteProgram", (uintptr_t)&glDeleteProgram },
  { "glDeleteRenderbuffers", (uintptr_t)&ret0 },
  { "glDeleteShader", (uintptr_t)&glDeleteShader },
  { "glDeleteTextures", (uintptr_t)&glDeleteTextures },
  { "glDepthFunc", (uintptr_t)&glDepthFunc },
  { "glDepthMask", (uintptr_t)&glDepthMask },
  { "glDisable", (uintptr_t)&glDisable },
  { "glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray },
  { "glDrawArrays", (uintptr_t)&glDrawArraysHook},
  { "glEnable", (uintptr_t)&glEnable },
  { "glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray },
  { "glFramebufferRenderbuffer", (uintptr_t)&ret0 },
  { "glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D },
  { "glGenFramebuffers", (uintptr_t)&glGenFramebuffers },
  { "glGenRenderbuffers", (uintptr_t)&ret0 },
  { "glGenTextures", (uintptr_t)&glGenTextures },
  { "glGetAttribLocation", (uintptr_t)&glGetAttribLocation },
  { "glGetError", (uintptr_t)&glGetError },
  { "glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog },
  { "glGetProgramiv", (uintptr_t)&glGetProgramiv },
  { "glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog },
  { "glGetShaderiv", (uintptr_t)&glGetShaderiv },
  { "glGetString", (uintptr_t)&glGetString },
  { "glGetUniformLocation", (uintptr_t)&glGetUniformLocation },
  { "glLineWidth", (uintptr_t)&glLineWidth },
  { "glLinkProgram", (uintptr_t)&glLinkProgramHook },
  { "glRenderbufferStorage", (uintptr_t)&ret0 },
  { "glScissor", (uintptr_t)&glScissor },
  { "glShaderSource", (uintptr_t)&glShaderSource },
  { "glTexImage2D", (uintptr_t)&glTexImage2D },
  { "glTexParameterf", (uintptr_t)&glTexParameterf },
  { "glTexParameteri", (uintptr_t)&glTexParameteri },
  { "glUniform1i", (uintptr_t)&glUniform1i },
  { "glUniform3fv", (uintptr_t)&glUniform3fv },
  { "glUniform4f", (uintptr_t)&glUniform4f },
  { "glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv },
  { "glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv },
  { "glUseProgram", (uintptr_t)&glUseProgramHook },
  { "glVertexAttrib2f", (uintptr_t)&glVertexAttrib2f },
  { "glVertexAttrib3f", (uintptr_t)&glVertexAttrib3f },
  { "glVertexAttrib4f", (uintptr_t)&glVertexAttrib4f },
  { "glVertexAttribPointer", (uintptr_t)&glVertexAttribPointerHook },
  { "glViewport", (uintptr_t)&glViewport },

  { "longjmp", (uintptr_t)&longjmp },
  { "setjmp", (uintptr_t)&setjmp },

  { "memchr", (uintptr_t)&memchr },
  { "memcmp", (uintptr_t)&memcmp },

  { "puts", (uintptr_t)&puts },
  // { "putchar", (uintptr_t)&putchar },
  { "qsort", (uintptr_t)&qsort },

  { "rand", (uintptr_t)&rand },
  { "srand", (uintptr_t)&srand },

  { "prctl", (uintptr_t)&ret0 },

  // { "close", (uintptr_t)&close },
  // { "closedir", (uintptr_t)&closedir },
  // { "lseek", (uintptr_t)&lseek },
  { "mkdir", (uintptr_t)&mkdir },
  // { "open", (uintptr_t)&open },
  // { "opendir", (uintptr_t)&opendir },
  // { "read", (uintptr_t)&read },
  // { "readdir", (uintptr_t)&readdir },
  { "stat", (uintptr_t)&stat },
  // { "write", (uintptr_t)&write },

  { "stderr", (uintptr_t)&stderr_fake },
  { "strcasecmp", (uintptr_t)&strcasecmp },
  { "strcat", (uintptr_t)&strcat },
  { "strchr", (uintptr_t)&strchr },
  { "strcmp", (uintptr_t)&strcmp },
  { "strcpy", (uintptr_t)&strcpy },
  { "strerror", (uintptr_t)&strerror },
  { "strlen", (uintptr_t)&strlen },
  { "strncasecmp", (uintptr_t)&strncasecmp },
  { "strncmp", (uintptr_t)&strncmp },
  { "strncpy", (uintptr_t)&strncpy },
  { "strpbrk", (uintptr_t)&strpbrk },
  { "strstr", (uintptr_t)&strstr },
  { "strtof", (uintptr_t)&strtof },
  { "strtol", (uintptr_t)&strtol },
  { "strtoul", (uintptr_t)&strtoul },

  { "toupper", (uintptr_t)&toupper },
  { "vasprintf", (uintptr_t)&vasprintf },

  // { "nanosleep", (uintptr_t)&nanosleep },
  { "usleep", (uintptr_t)&usleep },
};

int main(int argc, char *argv[]) {
  sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
  sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
  sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panelInfoFront);
  sceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &panelInfoBack);

  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);

  stderr_fake = stderr;

  so_load(SO_PATH);
  so_relocate();
  so_resolve(dynlib_functions, sizeof(dynlib_functions) / sizeof(DynLibFunction), 1);

  patch_openal();
  patch_opengl();
  patch_game();
  so_flush_caches();

  so_execute_init_array();
  so_free_temp();

  vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
  vglInitExtended(0, SCREEN_W, SCREEN_H, 16 * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X);
  vglUseVram(GL_TRUE);

  jni_load();

  return 0;
}
