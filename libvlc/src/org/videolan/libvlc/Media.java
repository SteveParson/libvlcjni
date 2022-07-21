/*****************************************************************************
 * Media.java
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
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

package org.videolan.libvlc;

import android.content.res.AssetFileDescriptor;
import android.net.Uri;

import androidx.annotation.Nullable;

import org.videolan.libvlc.interfaces.ILibVLC;
import org.videolan.libvlc.interfaces.IMedia;
import org.videolan.libvlc.interfaces.IMediaList;
import org.videolan.libvlc.util.AndroidUtil;
import org.videolan.libvlc.util.HWDecoderUtil;
import org.videolan.libvlc.util.VLCUtil;

import java.io.FileDescriptor;

@SuppressWarnings("unused, JniMissingFunction")
public class Media extends VLCObject<IMedia.Event> implements IMedia {
    private final static String TAG = "LibVLC/Media";

    @SuppressWarnings("unused") /* Used from JNI */
    private static Track createAudioTrackFromNative(String id, String codec, String originalCodec, int fourcc, int profile,
            int level, int bitrate, String language, String description,
            int channels, int rate) {
        return new AudioTrack(id, codec, originalCodec, fourcc, profile,
                level, bitrate, language, description,
                channels, rate);
    }

    @SuppressWarnings("unused") /* Used from JNI */
    private static Track createVideoTrackFromNative(String id, String codec, String originalCodec, int fourcc, int profile,
            int level, int bitrate, String language, String description,
            int height, int width, int sarNum, int sarDen, int frameRateNum, int frameRateDen,
            int orientation, int projection) {
        return new VideoTrack(id, codec, originalCodec, fourcc, profile,
                level, bitrate, language, description,
                height, width, sarNum, sarDen, frameRateNum, frameRateDen, orientation, projection);
    }

    @SuppressWarnings("unused") /* Used from JNI */
    private static Track createSubtitleTrackFromNative(String id, String codec, String originalCodec, int fourcc, int profile,
            int level, int bitrate, String language, String description,
            String encoding) {
        return new SubtitleTrack(id, codec, originalCodec, fourcc, profile,
                level, bitrate, language, description,
                encoding);
    }

    @SuppressWarnings("unused") /* Used from JNI */
    private static Track createUnknownTrackFromNative(String id, String codec, String originalCodec, int fourcc, int profile,
                                                      int level, int bitrate, String language, String description) {
        return new UnknownTrack(id, codec, originalCodec, fourcc, profile,
                level, bitrate, language, description);
    }

    @SuppressWarnings("unused") /* Used from JNI */
    private static Slave createSlaveFromNative(int type, int priority, String uri) {
        return new Slave(type, priority, uri);
    }

    @SuppressWarnings("unused") /* Used from JNI */
    private static Stats createStatsFromNative(int readBytes,
                                               float inputBitrate,
                                               int demuxReadBytes,
                                               float demuxBitrates,
                                               int demuxCorrupted,
                                               int demuxDiscontinuity,
                                               int decodedVideo,
                                               int decodedAudio,
                                               int displayedPictures,
                                               int lostPictures,
                                               int playedAbuffers,
                                               int lostAbuffers,
                                               int sentPackets,
                                               int sentBytes,
                                               float sendBitrate) {
        return new Stats(readBytes, inputBitrate, demuxReadBytes,
                         demuxBitrates, demuxCorrupted, demuxDiscontinuity,
                         decodedVideo, decodedAudio, displayedPictures,
                         lostPictures, playedAbuffers, lostAbuffers,
                         sentPackets, sentBytes, sendBitrate);
    }

    private static final int PARSE_STATUS_INIT = 0x00;
    private static final int PARSE_STATUS_PARSING = 0x01;
    private static final int PARSE_STATUS_PARSED = 0x02;

    private Uri mUri = null;
    private MediaList mSubItems = null;
    private int mParseStatus = PARSE_STATUS_INIT;
    private final String mNativeMetas[] = new String[Meta.MAX];
    private long mDuration = -1;
    private int mType = -1;
    private boolean mCodecOptionSet = false;
    private boolean mFileCachingSet = false;
    private boolean mNetworkCachingSet = false;


    /**
     * Create a Media from libVLC and a local path starting with '/'.
     *
     * @param ILibVLC a valid libVLC
     * @param path an absolute local path
     */
    public Media(ILibVLC ILibVLC, String path) {
        super(ILibVLC);
        nativeNewFromPath(ILibVLC, path);
        mUri = VLCUtil.UriFromMrl(nativeGetMrl());
    }

    /**
     * Create a Media from libVLC and a Uri
     *
     * @param ILibVLC a valid libVLC
     * @param uri a valid RFC 2396 Uri
     */
    public Media(ILibVLC ILibVLC, Uri uri) {
        super(ILibVLC);
        nativeNewFromLocation(ILibVLC, VLCUtil.encodeVLCUri(uri));
        mUri = uri;
    }

    /**
     * Create a Media from libVLC and a FileDescriptor
     *
     * @param ILibVLC a valid LibVLC
     * @param fd file descriptor object
     */
    public Media(ILibVLC ILibVLC, FileDescriptor fd) {
        super(ILibVLC);
        nativeNewFromFd(ILibVLC, fd);
        mUri = VLCUtil.UriFromMrl(nativeGetMrl());
    }

    /**
     * Create a Media from libVLC and an AssetFileDescriptor
     *
     * @param ILibVLC a valid LibVLC
     * @param afd asset file descriptor object
     */
    public Media(ILibVLC ILibVLC, AssetFileDescriptor afd) {
        super(ILibVLC);
        long offset = afd.getStartOffset();
        long length = afd.getLength();
        nativeNewFromFdWithOffsetLength(ILibVLC, afd.getFileDescriptor(), offset, length);
        mUri = VLCUtil.UriFromMrl(nativeGetMrl());
    }

    /**
     *
     * @param ml Should not be released and locked
     * @param index index of the Media from the MediaList
     */
    protected Media(IMediaList ml, int index) {
        super(ml);
        if (ml == null || ml.isReleased())
            throw new IllegalArgumentException("MediaList is null or released");
        if (!ml.isLocked())
            throw new IllegalStateException("MediaList should be locked");
        nativeNewFromMediaList(ml, index);
        mUri = VLCUtil.UriFromMrl(nativeGetMrl());
    }

    public void setEventListener(EventListener listener) {
        super.setEventListener(listener);
    }

    @Override
    protected synchronized Event onEventNative(int eventType, long arg1, long arg2, float argf1, @Nullable String args1) {
        switch (eventType) {
        case Event.MetaChanged:
            // either we update all metas (if first call) or we update a specific meta
            int id = (int) arg1;
            if (id >= 0 && id < Meta.MAX)
                mNativeMetas[id] = null;
            return new Event(eventType, arg1);
        case Event.DurationChanged:
            mDuration = -1;
            break;
        case Event.ParsedChanged:
            postParse();
            return new Event(eventType, arg1);
        }
        return new Event(eventType);
    }

    /**
     * Get the MRL associated with the Media.
     */
    public synchronized Uri getUri() {
        return mUri;
    }

    /**
     * Get the duration of the media.
     */
    public long getDuration() {
        synchronized (this) {
            if (mDuration != -1)
                return mDuration;
            if (isReleased())
                return 0;
        }
        final long duration = nativeGetDuration();
        synchronized (this) {
            mDuration = duration;
            return mDuration;
        }
    }

    /**
     * Get the subItems MediaList associated with the Media. This Media should be alive (not released).
     *
     * @return subItems as a MediaList. This MediaList should be released with {@link #release()}.
     */
    public MediaList subItems() {
        synchronized (this) {
            if (mSubItems != null) {
                mSubItems.retain();
                return mSubItems;
            }
        }
        final MediaList subItems = new MediaList(this);
        synchronized (this) {
            mSubItems = subItems;
            mSubItems.retain();
            return mSubItems;
        }
    }

    private synchronized void postParse() {
        // fetch if parsed and not fetched
        if ((mParseStatus & PARSE_STATUS_PARSED) != 0)
            return;
        mParseStatus &= ~PARSE_STATUS_PARSING;
        mParseStatus |= PARSE_STATUS_PARSED;
        mDuration = -1;
        mType = -1;
    }

    /**
     * Parse the media synchronously with a flag. This Media should be alive (not released).
     *
     * @param flags see {@link Parse}
     * @return true in case of success, false otherwise.
     */
    public boolean parse(int flags) {
        boolean parse = false;
        synchronized (this) {
            if ((mParseStatus & (PARSE_STATUS_PARSED | PARSE_STATUS_PARSING)) == 0) {
                mParseStatus |= PARSE_STATUS_PARSING;
                parse = true;
            }
        }
        if (parse && nativeParse(flags)) {
            postParse();
            return true;
        } else
            return false;
    }

    /**
     * Parse the media and local art synchronously. This Media should be alive (not released).
     *
     * @return true in case of success, false otherwise.
     */
    public boolean parse() {
        return parse(Parse.FetchLocal);
    }

    /**
     * Parse the media asynchronously with a flag. This Media should be alive (not released).
     *
     * To track when this is over you can listen to {@link Event#ParsedChanged}
     * event (only if this methods returned true).
     *
     * @param flags see {@link Parse}
     * @param timeout maximum time allowed to preparse the media. If -1, the
     * default "preparse-timeout" option will be used as a timeout. If 0, it will
     * wait indefinitely. If > 0, the timeout will be used (in milliseconds).
     * @return true in case of success, false otherwise.
     */
    public boolean parseAsync(int flags, int timeout) {
        boolean parse = false;
        synchronized (this) {
            if ((mParseStatus & (PARSE_STATUS_PARSED | PARSE_STATUS_PARSING)) == 0) {
                mParseStatus |= PARSE_STATUS_PARSING;
                parse = true;
            }
        }
        return parse && nativeParseAsync(flags, timeout);
    }

    public boolean parseAsync(int flags) {
        return parseAsync(flags, -1);
    }

    /**
     * Parse the media and local art asynchronously. This Media should be alive (not released).
     *
     * @see #parseAsync(int)
     */
    public boolean parseAsync() {
        return parseAsync(Parse.FetchLocal);
    }

    /**
     * Returns true if the media is parsed This Media should be alive (not released).
     */
    public synchronized boolean isParsed() {
        return (mParseStatus & PARSE_STATUS_PARSED) != 0;
    }

    /**
     * Get the type of the media
     *
     * @see {@link Type}
     */
    public int getType() {
        synchronized (this) {
            if (mType != -1)
                return mType;
            if (isReleased())
                return Type.Unknown;
        }
        final int type = nativeGetType();
        synchronized (this) {
            mType = type;
            return mType;
        }
    }

    /**
     * Get the list of tracks for a given type
     *
     * @param type type defined by {@link Media.Track.Type}
     * @return a track array or null. Each tracks can be casted to {@link
     * Media.VideoTrack}, {@link Media.AudioTrack}, {@link
     * Media.SubtitleTrack}, or {@link Media.UnknownTrack} depending on {@link
     * Media.type}
     */
    public Track[] getTracks(int type) {
        synchronized (this) {
            if (isReleased())
                return null;
        }
        return nativeGetTracks(type);
    }

    /**
     * Get the list of tracks for all types
     */
    public Track[] getTracks() {
        synchronized (this) {
            if (isReleased())
                return null;
        }

        Track[][] allTracksArray = new Track[4][];
        int allTracksCount = 0;

        for (int i = 0; i < 4; ++i) {
            allTracksArray[i] = nativeGetTracks(i - 1);
            allTracksCount += allTracksArray[i] != null ? allTracksArray[i].length : 0;
        }

        if (allTracksCount == 0)
            return null;

        Track[] allTracks = new Track[allTracksCount];
        allTracksCount = 0;
        for (int i = 0; i < 4; ++i) {
            if (allTracksArray[i] != null)
            {
                System.arraycopy(allTracksArray[i], 0, allTracks, allTracksCount, allTracksArray[i].length);
                allTracksCount += allTracksArray[i].length;
            }
        }

        return allTracks;
    }

    /**
     * Get a Meta.
     *
     * @param id see {@link Meta}
     * @return meta or null if not found
     */
    public String getMeta(int id) {
        return getMeta(id, false);
    }

    /**
     * Get a Meta.
     *
     * @param id see {@link Meta}
     * @param force force the native call to be done
     * @return meta or null if not found
     */
    public String getMeta(int id, boolean force) {
        if (id < 0 || id >= Meta.MAX)
            return null;

        if (!force) synchronized (this) {
            if (mNativeMetas[id] != null)
                return mNativeMetas[id];
            if (isReleased())
                return null;
        }

        final String meta = nativeGetMeta(id);
        synchronized (this) {
            mNativeMetas[id] = meta;
            return meta;
        }
    }


    private static String getMediaCodecModule() {
        return AndroidUtil.isLolliPopOrLater ? "mediacodec_ndk" : "mediacodec_jni";
    }

    /**
     * Add or remove hw acceleration media options
     *
     * @param enabled if true, hw decoder will be used
     * @param force force hw acceleration even for unknown devices
     */
    public void setHWDecoderEnabled(boolean enabled, boolean force) {

        if (LibVLC.majorVersion() == 3) {
            HWDecoderUtil.Decoder decoder = enabled ?
                    HWDecoderUtil.getDecoderFromDevice() :
                    HWDecoderUtil.Decoder.NONE;

            /* Unknown device but the user asked for hardware acceleration */
            if (decoder == HWDecoderUtil.Decoder.UNKNOWN && force)
                decoder = HWDecoderUtil.Decoder.ALL;

            if (decoder == HWDecoderUtil.Decoder.NONE || decoder == HWDecoderUtil.Decoder.UNKNOWN) {
                addOption(":codec=all");
                return;
            }

            /*
             * Set higher caching values if using iomx decoding, since some omx
             * decoders have a very high latency, and if the preroll data isn't
             * enough to make the decoder output a frame, the playback timing gets
             * started too soon, and every decoded frame appears to be too late.
             * On Nexus One, the decoder latency seems to be 25 input packets
             * for 320x170 H.264, a few packets less on higher resolutions.
             * On Nexus S, the decoder latency seems to be about 7 packets.
             */
            if (!mFileCachingSet)
                addOption(":file-caching=1500");
            if (!mNetworkCachingSet)
                addOption(":network-caching=1500");

            final StringBuilder sb = new StringBuilder(":codec=");
            if (decoder == HWDecoderUtil.Decoder.MEDIACODEC || decoder == HWDecoderUtil.Decoder.ALL)
                sb.append(getMediaCodecModule()).append(",");
            if (force && (decoder == HWDecoderUtil.Decoder.OMX || decoder == HWDecoderUtil.Decoder.ALL))
                sb.append("iomx,");
            sb.append("all");

            addOption(sb.toString());
        }
        else if (!enabled) /* LibVLC >= 4.0 */
            addOption(":no-hw-dec");
    }

    /**
     * Enable HWDecoder options if not already set
     */
    public void setDefaultMediaPlayerOptions() {
        if (LibVLC.majorVersion() == 3) {
            boolean codecOptionSet;
            synchronized (this) {
                codecOptionSet = mCodecOptionSet;
                mCodecOptionSet = true;
            }
            if (!codecOptionSet)
                setHWDecoderEnabled(true, false);
        }

        /* dvdnav need to be explicitly forced for network playbacks */
        if (mUri != null && mUri.getScheme() != null && !mUri.getScheme().equalsIgnoreCase("file") &&
                mUri.getLastPathSegment() != null && mUri.getLastPathSegment().toLowerCase().endsWith(".iso"))
            addOption(":demux=dvdnav,any");
    }

    /**
     * Add an option to this Media. This Media should be alive (not released).
     *
     * @param option ":option" or ":option=value"
     */
    public void addOption(String option) {
        synchronized (this) {
            if (!mCodecOptionSet && option.startsWith(":codec="))
                mCodecOptionSet = true;
            if (!mNetworkCachingSet && option.startsWith(":network-caching="))
                mNetworkCachingSet = true;
            if (!mFileCachingSet && option.startsWith(":file-caching="))
                mFileCachingSet = true;
        }
        nativeAddOption(option);
    }


    /**
     * Add a slave to the current media.
     *
     * A slave is an external input source that may contains an additional subtitle
     * track (like a .srt) or an additional audio track (like a .ac3).
     *
     * This function must be called before the media is parsed (via {@link #parseAsync(int)}} or
     * before the media is played (via {@link MediaPlayer#play()})
     */
    public void addSlave(Slave slave) {
        nativeAddSlave(slave.type, slave.priority, slave.uri);
    }

    /**
     * Clear all slaves previously added by {@link #addSlave(Slave)} or internally.
     */
    public void clearSlaves() {
        nativeClearSlaves();
    }

    /**
     * Get a media's slave list
     *
     * The list will contain slaves parsed by VLC or previously added by
     * {@link #addSlave(Slave)}. The typical use case of this function is to save
     * a list of slave in a database for a later use.
     */
    @Nullable
    public Slave[] getSlaves() {
        return nativeGetSlaves();
    }

    /**
     * Get the stats related to the playing media
     */
    @Nullable
    public Stats getStats() {
        return nativeGetStats();
    }

    @Override
    protected void onReleaseNative() {
        if (mSubItems != null)
            mSubItems.release();
        nativeRelease();
    }

    /* JNI */
    private native void nativeNewFromPath(ILibVLC ILibVLC, String path);
    private native void nativeNewFromLocation(ILibVLC ILibVLC, String location);
    private native void nativeNewFromFd(ILibVLC ILibVLC, FileDescriptor fd);
    private native void nativeNewFromFdWithOffsetLength(ILibVLC ILibVLC, FileDescriptor fd, long offset, long length);
    private native void nativeNewFromMediaList(IMediaList ml, int index);
    private native void nativeRelease();
    private native boolean nativeParseAsync(int flags, int timeout);
    private native boolean nativeParse(int flags);
    private native String nativeGetMrl();
    private native String nativeGetMeta(int id);
    private native Track[] nativeGetTracks(int type);
    private native long nativeGetDuration();
    private native int nativeGetType();
    private native void nativeAddOption(String option);
    private native void nativeAddSlave(int type, int priority, String uri);
    private native void nativeClearSlaves();
    private native Slave[] nativeGetSlaves();
    private native Stats nativeGetStats();
}
