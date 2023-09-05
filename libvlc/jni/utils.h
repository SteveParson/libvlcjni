/*****************************************************************************
 * utils.h
 *****************************************************************************
 * Copyright © 2012 VLC authors and VideoLAN
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

#ifndef LIBVLCJNI_UTILS_H
#define LIBVLCJNI_UTILS_H

#include <vlc/vlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>

#define LOG_TAG "VLC/JNI/VLCObject"
#include "log.h"


struct fields {
#define CLAZZ(name, fullname) jclass name##_clazz;
#define FIELD(clazz, name, args) jfieldID clazz##_##name;
#define METHOD(clazz, name, get_type, args) jmethodID clazz##_##name;
#include "jni_bindings.h"
#undef CLAZZ
#undef FIELD
#undef METHOD
};

static inline jstring vlcNewStringUTF(JNIEnv* env, const char* psz_string)
{
    if (psz_string == NULL)
        return NULL;
    for (int i = 0 ; psz_string[i] != '\0' ; ) {
        uint8_t lead = psz_string[i++];
        uint8_t nbBytes;
        if ((lead & 0x80) == 0)
            continue;
        else if ((lead >> 5) == 0x06)
            nbBytes = 1;
        else if ((lead >> 4) == 0x0E)
            nbBytes = 2;
        else if ((lead >> 3) == 0x1E)
            nbBytes = 3;
        else {
            LOGE("Invalid UTF lead character\n");
            return NULL;
        }
        for (int j = 0 ; j < nbBytes && psz_string[i] != '\0' ; j++) {
            uint8_t byte = psz_string[i++];
            if ((byte & 0x80) == 0) {
                LOGE("Invalid UTF byte\n");
                return NULL;
            }
        }
    }
    return (*env)->NewStringUTF(env, psz_string);
}

extern struct fields fields;

#endif // LIBVLCJNI_UTILS_H
