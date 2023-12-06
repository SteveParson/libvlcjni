/*****************************************************************************
 * jni_bindings.h.h
 *****************************************************************************
 * Copyright © 2023 VLC authors and VideoLAN
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

CLAZZ(IllegalStateException, "java/lang/IllegalStateException")
CLAZZ(IllegalArgumentException, "java/lang/IllegalArgumentException")
CLAZZ(RuntimeException, "java/lang/RuntimeException")
CLAZZ(OutOfMemoryError, "java/lang/OutOfMemoryError")
CLAZZ(String, "java/lang/String")
CLAZZ(FileDescriptor, "java/io/FileDescriptor")
CLAZZ(VLCObject, "org/videolan/libvlc/VLCObject")
CLAZZ(Media, "org/videolan/libvlc/Media")
CLAZZ(Media_Track, "org/videolan/libvlc/interfaces/IMedia$Track")
CLAZZ(Media_Slave, "org/videolan/libvlc/interfaces/IMedia$Slave")
CLAZZ(MediaPlayer, "org/videolan/libvlc/MediaPlayer")
CLAZZ(MediaPlayer_Title, "org/videolan/libvlc/MediaPlayer$Title")
CLAZZ(MediaPlayer_Chapter, "org/videolan/libvlc/MediaPlayer$Chapter")
CLAZZ(MediaPlayer_Equalizer, "org/videolan/libvlc/MediaPlayer$Equalizer")
CLAZZ(MediaDiscoverer, "org/videolan/libvlc/MediaDiscoverer")
CLAZZ(MediaDiscoverer_Description, "org/videolan/libvlc/MediaDiscoverer$Description")
CLAZZ(RendererDiscoverer, "org/videolan/libvlc/RendererDiscoverer")
CLAZZ(RendererDiscoverer_Description, "org/videolan/libvlc/RendererDiscoverer$Description")
CLAZZ(Dialog, "org/videolan/libvlc/Dialog")

FIELD(FileDescriptor, descriptor, "I")

FIELD(VLCObject, mInstance, "J")
METHOD(VLCObject, dispatchEventFromNative, GetMethodID, "(IJJFLjava/lang/String;)V")

METHOD(Media, createAudioTrackFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;"
    "Ljava/lang/String;IIIILjava/lang/String;Ljava/lang/String;II)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Track;")
METHOD(Media, createVideoTrackFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;"
    "Ljava/lang/String;IIIILjava/lang/String;Ljava/lang/String;IIIIIIII)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Track;")
METHOD(Media, createSubtitleTrackFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;"
    "Ljava/lang/String;IIIILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Track;")
METHOD(Media, createUnknownTrackFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;"
    "Ljava/lang/String;IIIILjava/lang/String;Ljava/lang/String;)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Track;")
METHOD(Media, createSlaveFromNative, GetStaticMethodID,
    "(IILjava/lang/String;)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Slave;")
METHOD(Media, createStatsFromNative, GetStaticMethodID,
    "(JFJFJJJJJJJJJJF)"
    "Lorg/videolan/libvlc/interfaces/IMedia$Stats;")

METHOD(MediaPlayer, createTitleFromNative, GetStaticMethodID,
    "(JLjava/lang/String;I)Lorg/videolan/libvlc/MediaPlayer$Title;")
METHOD(MediaPlayer, createChapterFromNative, GetStaticMethodID,
    "(JJLjava/lang/String;)Lorg/videolan/libvlc/MediaPlayer$Chapter;")

FIELD(MediaPlayer_Equalizer, mInstance, "J")

METHOD(MediaDiscoverer, createDescriptionFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;I)"
    "Lorg/videolan/libvlc/MediaDiscoverer$Description;")

METHOD(RendererDiscoverer, createDescriptionFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;)"
    "Lorg/videolan/libvlc/RendererDiscoverer$Description;")
METHOD(RendererDiscoverer, createItemFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IJ)"
    "Lorg/videolan/libvlc/RendererItem;")

METHOD(Dialog, displayErrorFromNative, GetStaticMethodID,
    "(Ljava/lang/String;Ljava/lang/String;)V")
METHOD(Dialog, displayLoginFromNative, GetStaticMethodID,
    "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)"
    "Lorg/videolan/libvlc/Dialog;")
METHOD(Dialog, displayQuestionFromNative, GetStaticMethodID,
    "(JLjava/lang/String;Ljava/lang/String;ILjava/lang/String;"
    "Ljava/lang/String;Ljava/lang/String;)"
    "Lorg/videolan/libvlc/Dialog;")
METHOD(Dialog, displayProgressFromNative, GetStaticMethodID,
    "(JLjava/lang/String;Ljava/lang/String;ZFLjava/lang/String;)"
    "Lorg/videolan/libvlc/Dialog;")
METHOD(Dialog, cancelFromNative, GetStaticMethodID,
    "(Lorg/videolan/libvlc/Dialog;)V")
METHOD(Dialog, updateProgressFromNative, GetStaticMethodID,
    "(Lorg/videolan/libvlc/Dialog;FLjava/lang/String;)V")
