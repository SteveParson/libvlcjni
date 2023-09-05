/*****************************************************************************
 * libvlcjni.c
 *****************************************************************************
 * Copyright © 2010-2013 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <vlc/vlc.h>

#include <jni.h>

#include <android/api-level.h>

#include "libvlcjni-vlcobject.h"
#include "utils.h"
#include "std_logger.h"

struct fields fields;

#define VLC_JNI_VERSION JNI_VERSION_1_2

#define THREAD_NAME "libvlcjni"
JNIEnv *jni_get_env(const char *name);

/* Pointer to the Java virtual machine
 * Note: It's okay to use a static variable for the VM pointer since there
 * can only be one instance of this shared library in a single VM
 */
static JavaVM *myVm;

static pthread_key_t jni_env_key;

/* This function is called when a thread attached to the Java VM is canceled or
 * exited */
static void jni_detach_thread(void *data)
{
    //JNIEnv *env = data;
    (*myVm)->DetachCurrentThread(myVm);
}

JNIEnv *jni_get_env(const char *name)
{
    JNIEnv *env;

    env = pthread_getspecific(jni_env_key);
    if (env == NULL) {
        /* if GetEnv returns JNI_OK, the thread is already attached to the
         * JavaVM, so we are already in a java thread, and we don't have to
         * setup any destroy callbacks */
        if ((*myVm)->GetEnv(myVm, (void **)&env, VLC_JNI_VERSION) != JNI_OK)
        {
            /* attach the thread to the Java VM */
            JavaVMAttachArgs args;
            jint result;

            args.version = VLC_JNI_VERSION;
            args.name = name;
            args.group = NULL;

            if ((*myVm)->AttachCurrentThread(myVm, &env, &args) != JNI_OK)
                return NULL;

            /* Set the attached env to the thread-specific data area (TSD) */
            if (pthread_setspecific(jni_env_key, env) != 0)
            {
                (*myVm)->DetachCurrentThread(myVm);
                return NULL;
            }
        }
    }

    return env;
}

#ifndef NDEBUG
static std_logger *p_std_logger = NULL;
#endif

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;
    // Keep a reference on the Java VM.
    myVm = vm;

    if ((*vm)->GetEnv(vm, (void**) &env, VLC_JNI_VERSION) != JNI_OK)
        return -1;

    /* Create a TSD area and setup a destroy callback when a thread that
     * previously set the jni_env_key is canceled or exited */
    if (pthread_key_create(&jni_env_key, jni_detach_thread) != 0)
        return -1;

#ifndef NDEBUG
    p_std_logger = std_logger_Open("VLC-std");
#endif

#define GET_CLASS(clazz, str) do { \
    jclass local_class = (*env)->FindClass(env, (str)); \
    if (!local_class) { \
        LOGE("FindClass(%s) failed", (str)); \
        return -1; \
    } \
    (clazz) = (jclass) (*env)->NewGlobalRef(env, local_class); \
    (*env)->DeleteLocalRef(env, local_class); \
    if (!(clazz)) { \
        LOGE("NewGlobalRef(%s) failed", (str)); \
        return -1; \
    } \
} while (0)

#define GET_ID(get, id, clazz, str, args) do { \
    (id) = (*env)->get(env, (clazz), (str), (args)); \
    if (!(id)) { \
        LOGE(#get"(%s) failed", (str)); \
        return -1; \
    } \
} while (0)

#define CLAZZ(name, fullname) \
    GET_CLASS(fields.name##_clazz, fullname);
#define FIELD(clazz, name, args) \
    GET_ID(GetFieldID, fields.clazz##_##name, fields.clazz##_clazz, #name, args);
#define METHOD(clazz, name, get_type, args) \
    GET_ID(get_type, fields.clazz##_##name, fields.clazz##_clazz, #name, args);
#include "jni_bindings.h"
#undef CLAZZ
#undef FIELD
#undef METHOD

#undef GET_CLASS
#undef GET_ID

    LOGD("JNI interface loaded.");
    return VLC_JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;

    if ((*vm)->GetEnv(vm, (void**) &env, VLC_JNI_VERSION) != JNI_OK)
        return;

#define CLAZZ(name, fullname) \
    (*env)->DeleteGlobalRef(env, fields.name##_clazz);
#define FIELD(clazz, name, args)
#define METHOD(clazz, name, get_type, args)
#include "jni_bindings.h"
#undef CLAZZ
#undef FIELD
#undef METHOD

    pthread_key_delete(jni_env_key);

#ifndef NDEBUG
    std_logger_Close(p_std_logger);
#endif
}

void Java_org_videolan_libvlc_LibVLC_nativeNew(JNIEnv *env, jobject thiz,
                                               jobjectArray jstringArray,
                                               jstring jhomePath)
{
    vlcjni_object *p_obj = NULL;
    libvlc_instance_t *p_libvlc = NULL;
    jstring *strings = NULL;
    const char **argv = NULL;
    int argc = 0;

    if (jhomePath)
    {
        const char *psz_home = (*env)->GetStringUTFChars(env, jhomePath, 0);
        if (psz_home)
        {
            setenv("HOME", psz_home, 1);
            (*env)->ReleaseStringUTFChars(env, jhomePath, psz_home);
        }
    }
    setenv("VLC_DATA_PATH", "/system/usr/share", 1);

    if (jstringArray)
    {
        argc = (*env)->GetArrayLength(env, jstringArray);

        argv = malloc(argc * sizeof(const char *));
        strings = malloc(argc * sizeof(jstring));
        if (!argv || !strings)
        {
            argc = 0;
            goto error;
        }
        for (int i = 0; i < argc; ++i)
        {
            strings[i] = (*env)->GetObjectArrayElement(env, jstringArray, i);
            if (!strings[i])
            {
                argc = i;
                goto error;
            }
            argv[i] = (*env)->GetStringUTFChars(env, strings[i], 0);
            if (!argv)
            {
                argc = i;
                goto error;
            }
        }
    }

    p_libvlc = libvlc_new(argc, argv);

error:

    if (jstringArray)
    {
        for (int i = 0; i < argc; ++i)
        {
            (*env)->ReleaseStringUTFChars(env, strings[i], argv[i]);
            (*env)->DeleteLocalRef(env, strings[i]);
        }
    }
    free(argv);
    free(strings);

    if (!p_libvlc)
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_STATE,
                        "can't create LibVLC instance");
        return;
    }

    p_obj = VLCJniObject_newFromLibVlc(env, thiz, NULL);
    if (!p_obj)
    {
        libvlc_release(p_libvlc);
        return;
    }
    p_obj->u.p_libvlc = p_libvlc;
}

void Java_org_videolan_libvlc_LibVLC_nativeRelease(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    libvlc_release(p_obj->u.p_libvlc);
}

jstring Java_org_videolan_libvlc_LibVLC_version(JNIEnv* env, jobject thiz)
{
    return vlcNewStringUTF(env, libvlc_get_version());
}

jint Java_org_videolan_libvlc_LibVLC_majorVersion(JNIEnv* env, jobject thiz)
{
    return atoi(libvlc_get_version());
}

jstring Java_org_videolan_libvlc_LibVLC_compiler(JNIEnv* env, jobject thiz)
{
    return vlcNewStringUTF(env, libvlc_get_compiler());
}

jstring Java_org_videolan_libvlc_LibVLC_changeset(JNIEnv* env, jobject thiz)
{
    return vlcNewStringUTF(env, libvlc_get_changeset());
}

void Java_org_videolan_libvlc_LibVLC_nativeSetUserAgent(JNIEnv* env,
                                                        jobject thiz,
                                                        jstring jname,
                                                        jstring jhttp)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    const char *psz_name, *psz_http;

    if (!p_obj)
        return;

    psz_name = jname ? (*env)->GetStringUTFChars(env, jname, 0) : NULL;
    psz_http = jhttp ? (*env)->GetStringUTFChars(env, jhttp, 0) : NULL;

    if (psz_name && psz_http)
        libvlc_set_user_agent(p_obj->u.p_libvlc, psz_name, psz_http);

    if (psz_name)
        (*env)->ReleaseStringUTFChars(env, jname, psz_name);
    if (psz_http)
        (*env)->ReleaseStringUTFChars(env, jhttp, psz_http);

    if (!psz_name || !psz_http)
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "name or http invalid");
}
