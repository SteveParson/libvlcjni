/*****************************************************************************
 * libvlcjni-mediaplayer.c
 *****************************************************************************
 * Copyright © 2010-2015 VLC authors and VideoLAN
 *
 * Authors:     Jean-Baptiste Kempf
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

#include <pthread.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "libvlcjni-vlcobject.h"

#define THREAD_NAME "libvlcjni"
JNIEnv *jni_get_env(const char *name);

static const libvlc_event_type_t mp_events[] = {
    libvlc_MediaPlayerMediaChanged,
    libvlc_MediaPlayerOpening,
    libvlc_MediaPlayerBuffering,
    libvlc_MediaPlayerPlaying,
    libvlc_MediaPlayerPaused,
    libvlc_MediaPlayerStopped,
#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    libvlc_MediaPlayerStopping,
#else
    libvlc_MediaPlayerEndReached,
#endif
    libvlc_MediaPlayerEncounteredError,
    libvlc_MediaPlayerTimeChanged,
    libvlc_MediaPlayerPositionChanged,
    libvlc_MediaPlayerVout,
    libvlc_MediaPlayerESAdded,
    libvlc_MediaPlayerESDeleted,
    libvlc_MediaPlayerESSelected,
    libvlc_MediaPlayerSeekableChanged,
    libvlc_MediaPlayerPausableChanged,
    libvlc_MediaPlayerLengthChanged,
    libvlc_MediaPlayerRecordChanged,
    -1,
};

struct vlcjni_object_sys
{
    jobject jwindow;
    libvlc_video_viewpoint_t *p_vp;

#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    pthread_mutex_t     stop_lock;
    pthread_cond_t      stop_cond;
    bool                stopped;
#endif
};

static libvlc_equalizer_t *
Equalizer_getInstance(JNIEnv *env, jobject thiz)
{
    intptr_t i_ptr = (intptr_t)
        (*env)->GetLongField(env, thiz,
                             fields.MediaPlayer_Equalizer_mInstance);
    if (!i_ptr)
        throw_Exception(env, VLCJNI_EX_ILLEGAL_STATE,
                        "can't get Equalizer instance");
    return (libvlc_equalizer_t*) i_ptr;
}

static void
VLCJniObject_setInstance(JNIEnv *env, jobject thiz, libvlc_equalizer_t *p_eq)
{
    (*env)->SetLongField(env, thiz,
                         fields.MediaPlayer_Equalizer_mInstance,
                         (jlong)(intptr_t)p_eq);
}

static bool
MediaPlayer_event_cb(vlcjni_object *p_obj, const libvlc_event_t *p_ev,
                     java_event *p_java_event)
{
    switch (p_ev->type)
    {
#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
        case libvlc_MediaPlayerStopped:
            pthread_mutex_lock(&p_obj->p_sys->stop_lock);
            if (!p_obj->p_sys->stopped)
            {
                p_obj->p_sys->stopped = true;
                pthread_cond_signal(&p_obj->p_sys->stop_cond);
            }
            pthread_mutex_unlock(&p_obj->p_sys->stop_lock);
            break;
#endif
        case libvlc_MediaPlayerBuffering:
            p_java_event->argf1 = p_ev->u.media_player_buffering.new_cache;
            break;
        case libvlc_MediaPlayerPositionChanged:
            p_java_event->argf1 = p_ev->u.media_player_position_changed.new_position;
            break;
        case libvlc_MediaPlayerTimeChanged:
            p_java_event->arg1 = p_ev->u.media_player_time_changed.new_time;
            break;
        case libvlc_MediaPlayerVout:
            p_java_event->arg1 = p_ev->u.media_player_vout.new_count;
            break;
        case libvlc_MediaPlayerESAdded:
        case libvlc_MediaPlayerESDeleted:
        case libvlc_MediaPlayerESSelected:
            p_java_event->arg1 = p_ev->u.media_player_es_changed.i_type;
            p_java_event->arg2 = p_ev->u.media_player_es_changed.i_id;
            break;
        case libvlc_MediaPlayerSeekableChanged:
            p_java_event->arg1 = p_ev->u.media_player_seekable_changed.new_seekable;
            break;
        case libvlc_MediaPlayerPausableChanged:
            p_java_event->arg1 = p_ev->u.media_player_pausable_changed.new_pausable;
            break;
        case libvlc_MediaPlayerLengthChanged:
            p_java_event->arg1 = p_ev->u.media_player_length_changed.new_length;
            break;
        case libvlc_MediaPlayerRecordChanged:
            p_java_event->arg1 = p_ev->u.media_player_record_changed.recording;
            p_java_event->argc1 = p_ev->u.media_player_record_changed.recorded_file_path;
            break;
    }
    p_java_event->type = p_ev->type;
    return true;
}

static void
MediaPlayer_newCommon(JNIEnv *env, jobject thiz, vlcjni_object *p_obj,
                      jobject jwindow)
{
    p_obj->p_sys = calloc(1, sizeof(vlcjni_object_sys));

    if (!p_obj->u.p_mp || !p_obj->p_sys)
    {
        VLCJniObject_release(env, thiz, p_obj);
        throw_Exception(env,
                        !p_obj->u.p_mp ? VLCJNI_EX_ILLEGAL_STATE : VLCJNI_EX_OUT_OF_MEMORY,
                        "can't create MediaPlayer instance");
        return;
    }
    p_obj->p_sys->jwindow = (*env)->NewGlobalRef(env, jwindow);
    if (!p_obj->p_sys->jwindow)
    {
        VLCJniObject_release(env, thiz, p_obj);
        throw_Exception(env, VLCJNI_EX_ILLEGAL_STATE,
                             "can't create MediaPlayer instance");
        return;
    }
    libvlc_media_player_set_android_context(p_obj->u.p_mp, p_obj->p_sys->jwindow);

#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    pthread_mutex_init(&p_obj->p_sys->stop_lock, NULL);
    pthread_cond_init(&p_obj->p_sys->stop_cond, NULL);
    p_obj->p_sys->stopped = true;
#endif

    VLCJniObject_attachEvents(p_obj, MediaPlayer_event_cb,
                              libvlc_media_player_event_manager(p_obj->u.p_mp),
                              mp_events);
}


void
Java_org_videolan_libvlc_MediaPlayer_nativeNewFromLibVlc(JNIEnv *env,
                                                         jobject thiz,
                                                         jobject libvlc,
                                                         jobject jwindow)
{
    vlcjni_object *p_obj = VLCJniObject_newFromJavaLibVlc(env, thiz, libvlc);
    if (!p_obj)
        return;

    /* Create a media player playing environment */
    p_obj->u.p_mp = libvlc_media_player_new(p_obj->p_libvlc);
    MediaPlayer_newCommon(env, thiz, p_obj, jwindow);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeNewFromMedia(JNIEnv *env,
                                                        jobject thiz,
                                                        jobject jmedia,
                                                        jobject jwindow)
{
    vlcjni_object *p_obj;
    vlcjni_object *p_m_obj = VLCJniObject_getInstance(env, jmedia);

    if (!p_m_obj)
        return;

    p_obj = VLCJniObject_newFromLibVlc(env, thiz, p_m_obj->p_libvlc);
    if (!p_obj)
        return;
    p_obj->u.p_mp = libvlc_media_player_new_from_media(p_m_obj->p_libvlc,
                                                       p_m_obj->u.p_m);
    MediaPlayer_newCommon(env, thiz, p_obj, jwindow);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeRelease(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_release(p_obj->u.p_mp);

    if (p_obj->p_sys && p_obj->p_sys->jwindow)
        (*env)->DeleteGlobalRef(env, p_obj->p_sys->jwindow);

    free(p_obj->p_sys->p_vp);

#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    pthread_mutex_destroy(&p_obj->p_sys->stop_lock);
    pthread_cond_destroy(&p_obj->p_sys->stop_cond);
#endif

    free(p_obj->p_sys);

    VLCJniObject_release(env, thiz, p_obj);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetMedia(JNIEnv *env,
                                                    jobject thiz,
                                                    jobject jmedia)
{
    libvlc_media_t *p_m = NULL;
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    if (jmedia)
    {
        vlcjni_object *p_m_obj = VLCJniObject_getInstance(env, jmedia);

        if (!p_m_obj)
            return;
        p_m = p_m_obj->u.p_m;
    }

    libvlc_media_player_set_media(p_obj->u.p_mp, p_m);
}

jint
Java_org_videolan_libvlc_MediaPlayer_nativeSetRenderer(JNIEnv *env,
                                                       jobject thiz,
                                                       jobject jrenderer)
{
    libvlc_renderer_item_t *p_m = NULL;
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    if (!p_obj)
        return -1;
    if (jrenderer)
    {
        vlcjni_object *p_m_obj = VLCJniObject_getInstance(env, jrenderer);

        if (!p_m_obj)
            return -1;
        p_m = p_m_obj->u.p_r;
    }
    return libvlc_media_player_set_renderer(p_obj->u.p_mp, p_m);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetVideoTitleDisplay(JNIEnv *env,
                                                                jobject thiz,
                                                                jint jposition,
                                                                jint jtimeout)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_video_title_display(p_obj->u.p_mp, jposition,
                                                jtimeout);
}

jfloat
Java_org_videolan_libvlc_MediaPlayer_getRate(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return 0.0f;

    return libvlc_media_player_get_rate(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_setRate(JNIEnv *env, jobject thiz,
                                                  jfloat rate)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_rate(p_obj->u.p_mp, rate);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_isPlaying(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    return !!libvlc_media_player_is_playing(p_obj->u.p_mp);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_isSeekable(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    return !!libvlc_media_player_is_seekable(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativePlay(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_play(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeStop(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    /* XXX: temporary during the VLC 3.0 -> 4.0 transition. The Java API need
     * to be updated to handle async stop (only?). */
    pthread_mutex_lock(&p_obj->p_sys->stop_lock);
    p_obj->p_sys->stopped = false;
    int ret = libvlc_media_player_stop_async(p_obj->u.p_mp);
    if (ret == 0)
        while (!p_obj->p_sys->stopped)
            pthread_cond_wait(&p_obj->p_sys->stop_cond, &p_obj->p_sys->stop_lock);
    else
        p_obj->p_sys->stopped = true;
    pthread_mutex_unlock(&p_obj->p_sys->stop_lock);
#else
    libvlc_media_player_stop(p_obj->u.p_mp);
#endif
}

void
Java_org_videolan_libvlc_MediaPlayer_pause(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_pause(p_obj->u.p_mp, 1);
}

jint
Java_org_videolan_libvlc_MediaPlayer_getPlayerState(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return (jint) libvlc_media_player_get_state(p_obj->u.p_mp);
}

jint
Java_org_videolan_libvlc_MediaPlayer_getVolume(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return (jint) libvlc_audio_get_volume(p_obj->u.p_mp);
}

/* Returns 0 if the volume was set, -1 if it was out of range or error */
jint
Java_org_videolan_libvlc_MediaPlayer_setVolume(JNIEnv *env, jobject thiz,
                                               jint volume)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return (jint) libvlc_audio_set_volume(p_obj->u.p_mp, (int) volume);
}

jlong
Java_org_videolan_libvlc_MediaPlayer_getTime(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return libvlc_media_player_get_time(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetTime(JNIEnv *env, jobject thiz,
                                                   jlong time, jboolean fast)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_time(p_obj->u.p_mp, time, fast);
}

jfloat
Java_org_videolan_libvlc_MediaPlayer_getPosition(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return (jfloat) libvlc_media_player_get_position(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetPosition(JNIEnv *env, jobject thiz,
                                                       jfloat pos, jboolean fast)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_position(p_obj->u.p_mp, pos, fast);

}

jlong
Java_org_videolan_libvlc_MediaPlayer_getLength(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return (jlong) libvlc_media_player_get_length(p_obj->u.p_mp);
}

jint
Java_org_videolan_libvlc_MediaPlayer_getTitle(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return libvlc_media_player_get_title(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_setTitle(JNIEnv *env, jobject thiz,
                                              jint title)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_title(p_obj->u.p_mp, title);
}

jint
Java_org_videolan_libvlc_MediaPlayer_getChapter(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return -1;

    return libvlc_media_player_get_chapter(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_setChapter(JNIEnv *env, jobject thiz,
                                                jint chapter)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_set_chapter(p_obj->u.p_mp, chapter);
}

void
Java_org_videolan_libvlc_MediaPlayer_previousChapter(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_previous_chapter(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nextChapter(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_next_chapter(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_navigate(JNIEnv *env, jobject thiz,
                                                    jint navigate)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_navigate(p_obj->u.p_mp, (unsigned) navigate);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSetAudioOutput(JNIEnv *env,
                                                          jobject thiz,
                                                          jstring jaout)
{
    const char* psz_aout;
    int i_ret;
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    if (!jaout || !(psz_aout = (*env)->GetStringUTFChars(env, jaout, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "aout invalid");
        return false;
    }

    i_ret = libvlc_audio_output_set(p_obj->u.p_mp, psz_aout);
    (*env)->ReleaseStringUTFChars(env, jaout, psz_aout);

    return i_ret == 0 ? true : false;
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSetAudioOutputDevice(JNIEnv *env,
                                                                jobject thiz,
                                                                jstring jid)
{
    const char* psz_id;
    int i_ret;
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    if (!jid || !(psz_id = (*env)->GetStringUTFChars(env, jid, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "aout invalid");
        return false;
    }

#if defined(LIBVLC_VERSION_MAJOR) && LIBVLC_VERSION_MAJOR >= 4
    libvlc_audio_output_device_set(p_obj->u.p_mp, psz_id);
#else
    libvlc_audio_output_device_set(p_obj->u.p_mp, NULL, psz_id);
#endif
    (*env)->ReleaseStringUTFChars(env, jid, psz_id);
    return true;
}

static jobject
mediaplayer_title_to_object(JNIEnv *env, libvlc_title_description_t *p_title)
{
    jstring jname = NULL;

    if (!p_title)
        return NULL;

    if (p_title->psz_name)
        jname = vlcNewStringUTF(env, p_title->psz_name);

    jobject jobj = (*env)->CallStaticObjectMethod(env, fields.MediaPlayer_clazz,
                        fields.MediaPlayer_createTitleFromNative,
                        p_title->i_duration,
                        jname,
                        p_title->i_flags);

    if (jname)
        (*env)->DeleteLocalRef(env, jname);
    return jobj;
}

jobject
Java_org_videolan_libvlc_MediaPlayer_nativeGetTitles(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    libvlc_title_description_t **pp_titles = NULL;
    int i_nb_titles;
    jobjectArray array;

    if (!p_obj)
        return NULL;

    i_nb_titles = libvlc_media_player_get_full_title_descriptions(p_obj->u.p_mp,
                                                                  &pp_titles);
    if (i_nb_titles <= 0)
        return NULL;

    array = (*env)->NewObjectArray(env, i_nb_titles,
                                   fields.MediaPlayer_Title_clazz, NULL);
    if (!array)
        goto error;

    for (int i = 0; i < i_nb_titles; ++i)
    {
        jobject jtitle = mediaplayer_title_to_object(env, pp_titles[i]);

        if (jtitle)
        {
            (*env)->SetObjectArrayElement(env, array, i, jtitle);
            (*env)->DeleteLocalRef(env, jtitle);
        }
    }

error:
    if (pp_titles)
        libvlc_title_descriptions_release(pp_titles, i_nb_titles);
    return array;
}

static jobject
mediaplayer_chapter_to_object(JNIEnv *env,
                              libvlc_chapter_description_t *p_chapter)
{
    jstring jname = NULL;

    if (!p_chapter)
        return NULL;

    if (p_chapter->psz_name)
        jname = vlcNewStringUTF(env, p_chapter->psz_name);

    jobject jobj = (*env)->CallStaticObjectMethod(env, fields.MediaPlayer_clazz,
                        fields.MediaPlayer_createChapterFromNative,
                        p_chapter->i_time_offset,
                        p_chapter->i_duration,
                        jname);

    if (jname)
        (*env)->DeleteLocalRef(env, jname);
    return jobj;
}

jobject
Java_org_videolan_libvlc_MediaPlayer_nativeGetChapters(JNIEnv *env,
                                                       jobject thiz,
                                                       jint jtitle)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    libvlc_chapter_description_t **pp_chapters = NULL;
    int i_nb_chapters;
    jobjectArray array;

    if (!p_obj)
        return NULL;

    i_nb_chapters =
        libvlc_media_player_get_full_chapter_descriptions(p_obj->u.p_mp,
                                                          jtitle, &pp_chapters);
    if (i_nb_chapters <= 0)
        return NULL;

    array = (*env)->NewObjectArray(env, i_nb_chapters,
                                   fields.MediaPlayer_Chapter_clazz, NULL);
    if (!array)
        goto error;

    for (int i = 0; i < i_nb_chapters; ++i)
    {
        jobject jchapter = mediaplayer_chapter_to_object(env, pp_chapters[i]);

        if (jchapter)
        {
            (*env)->SetObjectArrayElement(env, array, i, jchapter);
            (*env)->DeleteLocalRef(env, jchapter);
        }
    }

error:
    if (pp_chapters)
        libvlc_chapter_descriptions_release(pp_chapters, i_nb_chapters);
    return array;
}

jobject
Java_org_videolan_libvlc_MediaPlayer_nativeGetTracks(JNIEnv *env, jobject thiz,
                                                     jint type, jboolean selected)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return NULL;

    libvlc_media_tracklist_t *tracklist =
        libvlc_media_player_get_tracklist(p_obj->u.p_mp, type, selected);
    if (tracklist == NULL)
        return NULL;

    jobject array = tracklist_to_jobjectArray(env, tracklist);

    libvlc_media_tracklist_delete(tracklist);

    return array;
}

jobject
Java_org_videolan_libvlc_MediaPlayer_nativeGetSelectedTrack(JNIEnv *env,
                                                            jobject thiz,
                                                            jint type)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return NULL;

    libvlc_media_track_t *track =
        libvlc_media_player_get_selected_track(p_obj->u.p_mp, type);

    if (track == NULL)
        return NULL;

    jobject jtrack = media_track_to_jobject(env, track);

    libvlc_media_track_release(track);

    return jtrack;
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSelectTrack(JNIEnv *env,
                                                       jobject thiz,
                                                       jstring jtrack)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    const char *psz_track;
    if (!jtrack || !(psz_track = (*env)->GetStringUTFChars(env, jtrack, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "track str invalid");
        return false;
    }

    libvlc_media_track_t *track =
        libvlc_media_player_get_track_from_id(p_obj->u.p_mp, psz_track);

    (*env)->ReleaseStringUTFChars(env, jtrack, psz_track);

    if (track == NULL)
        return false;

    libvlc_media_player_select_track(p_obj->u.p_mp, track);
    libvlc_media_track_release(track);

    return true;
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSelectTracks(JNIEnv *env,
                                                        jobject thiz,
                                                        jint type,
                                                        jstring jtracks)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    const char *psz_tracks = NULL;
    if (jtracks && !(psz_tracks = (*env)->GetStringUTFChars(env, jtracks, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "tracks str invalid");
        return;
    }

    libvlc_media_player_select_tracks_by_ids(p_obj->u.p_mp, type, psz_tracks);

    (*env)->ReleaseStringUTFChars(env, jtracks, psz_tracks);
}
void
Java_org_videolan_libvlc_MediaPlayer_nativeUnselectTrackType(JNIEnv *env,
                                                             jobject thiz,
                                                             jint type)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_media_player_unselect_track_type(p_obj->u.p_mp, type);
}


jlong
Java_org_videolan_libvlc_MediaPlayer_nativeGetAudioDelay(JNIEnv *env,
                                                         jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return 0;

    return libvlc_audio_get_delay(p_obj->u.p_mp);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSetAudioDelay(JNIEnv *env,
                                                         jobject thiz,
                                                         jlong delay)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return false;

    return libvlc_audio_set_delay(p_obj->u.p_mp, delay) == 0 ? true : false;
}

jlong
Java_org_videolan_libvlc_MediaPlayer_nativeGetSpuDelay(JNIEnv *env,
                                                       jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return 0;

    return libvlc_video_get_spu_delay(p_obj->u.p_mp);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSetSpuDelay(JNIEnv *env,
                                                       jobject thiz,
                                                       jlong delay)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
       return false;

    return libvlc_video_set_spu_delay(p_obj->u.p_mp, delay) == 0 ? true : false;
}

float
Java_org_videolan_libvlc_MediaPlayer_nativeGetScale(JNIEnv *env, jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return 0.f;

    return libvlc_video_get_scale(p_obj->u.p_mp);
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetScale(JNIEnv *env, jobject thiz,
                                                    jfloat factor)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    libvlc_video_set_scale(p_obj->u.p_mp, factor);
}

jstring
Java_org_videolan_libvlc_MediaPlayer_nativeGetAspectRatio(JNIEnv *env,
                                                          jobject thiz)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return NULL;

    char *psz_aspect = libvlc_video_get_aspect_ratio(p_obj->u.p_mp);
    jstring jaspect = psz_aspect ? vlcNewStringUTF(env, psz_aspect) : NULL;
    free(psz_aspect);
    return jaspect;
}

void
Java_org_videolan_libvlc_MediaPlayer_nativeSetAspectRatio(JNIEnv *env,
                                                          jobject thiz,
                                                          jstring jaspect)
{
    const char* psz_aspect;
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);

    if (!p_obj)
        return;

    if (!jaspect)
    {
        libvlc_video_set_aspect_ratio(p_obj->u.p_mp, NULL);
        return;
    }
    if (!(psz_aspect = (*env)->GetStringUTFChars(env, jaspect, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "aspect invalid");
        return;
    }

    libvlc_video_set_aspect_ratio(p_obj->u.p_mp, psz_aspect);
    (*env)->ReleaseStringUTFChars(env, jaspect, psz_aspect);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeUpdateViewpoint(JNIEnv *env,
                                                           jobject thiz,
                                                           jfloat yaw,
                                                           jfloat pitch,
                                                           jfloat roll,
                                                           jfloat fov,
                                                           jboolean absolute)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    if (!p_obj)
        return false;

    if (p_obj->p_sys->p_vp == NULL)
    {
        p_obj->p_sys->p_vp = libvlc_video_new_viewpoint();
        if (p_obj->p_sys->p_vp == NULL)
            return false;
    }
    p_obj->p_sys->p_vp->f_yaw = yaw;
    p_obj->p_sys->p_vp->f_pitch = pitch;
    p_obj->p_sys->p_vp->f_roll = roll;
    p_obj->p_sys->p_vp->f_field_of_view = fov;

    return libvlc_video_update_viewpoint(p_obj->u.p_mp, p_obj->p_sys->p_vp,
                                         absolute) == 0 ? true : false;
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeAddSlave(JNIEnv *env,
                                                    jobject thiz, jint type,
                                                    jstring jmrl, jboolean select)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    const char* psz_mrl;

    if (!p_obj)
        return false;

    if (!jmrl || !(psz_mrl = (*env)->GetStringUTFChars(env, jmrl, 0)))
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "mrl invalid");
        return false;
    }

    jboolean ret = libvlc_media_player_add_slave(p_obj->u.p_mp, type, psz_mrl, select) == 0;

    (*env)->ReleaseStringUTFChars(env, jmrl, psz_mrl);
    return ret;
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeSetEqualizer(JNIEnv *env,
                                                        jobject thiz,
                                                        jobject jequalizer)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    libvlc_equalizer_t *p_eq = NULL;

    if (!p_obj)
       return false;

    if (jequalizer)
    {
        p_eq = Equalizer_getInstance(env, jequalizer);
        if (!p_eq)
            return false;
    }

    return libvlc_media_player_set_equalizer(p_obj->u.p_mp, p_eq) == 0 ? true: false;
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_nativeRecord(JNIEnv *env, jobject thiz,
                                                  jstring jdirectory)
{
    vlcjni_object *p_obj = VLCJniObject_getInstance(env, thiz);
    const char *psz_directory;

    if (!p_obj)
        return false;

    int (*record_func)(libvlc_media_player_t *, const char *) =
        dlsym(RTLD_DEFAULT, "libvlc_media_player_record");

    if (!record_func)
        return false;

    if (jdirectory)
    {
        psz_directory = (*env)->GetStringUTFChars(env, jdirectory, 0);
        if (!psz_directory)
        {
            throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "directory invalid");
            return false;
        }
    }
    else
        psz_directory = NULL;

    jboolean ret = record_func(p_obj->u.p_mp, psz_directory) == 0;

    if (psz_directory)
    {
        (*env)->ReleaseStringUTFChars(env, jdirectory, psz_directory);
    }

    return ret;
}

jint
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetPresetCount(JNIEnv *env,
                                                                         jobject thiz)
{
    return libvlc_audio_equalizer_get_preset_count();
}

jstring
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetPresetName(JNIEnv *env,
                                                                        jobject thiz,
                                                                        jint index)
{
    const char *psz_name;

    if (index < 0)
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "index invalid");
        return NULL;
    }

    psz_name = libvlc_audio_equalizer_get_preset_name(index);

    return psz_name ? vlcNewStringUTF(env, psz_name) : NULL;
}

jint
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetBandCount(JNIEnv *env,
                                                                       jobject thiz)
{
    return libvlc_audio_equalizer_get_band_count();
}

jfloat
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetBandFrequency(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jint index)
{
    if (index < 0)
    {
        throw_Exception(env, VLCJNI_EX_ILLEGAL_ARGUMENT, "index invalid");
        return 0.0;
    }

    return libvlc_audio_equalizer_get_band_frequency(index);
}

void
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeNew(JNIEnv *env,
                                                              jobject thiz)
{
    libvlc_equalizer_t *p_eq = libvlc_audio_equalizer_new();
    if (!p_eq)
        throw_Exception(env, VLCJNI_EX_OUT_OF_MEMORY, "Equalizer");

    VLCJniObject_setInstance(env, thiz, p_eq);
}

void
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeNewFromPreset(JNIEnv *env,
                                                                        jobject thiz,
                                                                        jint index)
{
    libvlc_equalizer_t *p_eq = libvlc_audio_equalizer_new_from_preset(index);
    if (!p_eq)
        throw_Exception(env, VLCJNI_EX_OUT_OF_MEMORY, "Equalizer");

    VLCJniObject_setInstance(env, thiz, p_eq);
}

void
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeRelease(JNIEnv *env,
                                                                  jobject thiz)
{
    libvlc_equalizer_t *p_eq = Equalizer_getInstance(env, thiz);
    if (!p_eq)
        return;

    libvlc_audio_equalizer_release(p_eq);
    VLCJniObject_setInstance(env, thiz, NULL);
}

jfloat
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetPreAmp(JNIEnv *env,
                                                                    jobject thiz)
{
    libvlc_equalizer_t *p_eq = Equalizer_getInstance(env, thiz);
    if (!p_eq)
        return 0.0;

    return libvlc_audio_equalizer_get_preamp(p_eq);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeSetPreAmp(JNIEnv *env,
                                                                    jobject thiz,
                                                                    jfloat preamp)
{
    libvlc_equalizer_t *p_eq = Equalizer_getInstance(env, thiz);
    if (!p_eq)
        return false;

    return libvlc_audio_equalizer_set_preamp(p_eq, preamp) == 0 ? true : false;
}

jfloat
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeGetAmp(JNIEnv *env,
                                                                 jobject thiz,
                                                                 jint index)
{
    libvlc_equalizer_t *p_eq = Equalizer_getInstance(env, thiz);
    if (!p_eq)
        return 0.0;

    return libvlc_audio_equalizer_get_amp_at_index(p_eq, index);
}

jboolean
Java_org_videolan_libvlc_MediaPlayer_00024Equalizer_nativeSetAmp(JNIEnv *env,
                                                                 jobject thiz,
                                                                 jint index,
                                                                 jfloat amp)
{
    libvlc_equalizer_t *p_eq = Equalizer_getInstance(env, thiz);
    if (!p_eq)
        return false;

    return libvlc_audio_equalizer_set_amp_at_index(p_eq, amp, index) == 0 ? true : false;
}
