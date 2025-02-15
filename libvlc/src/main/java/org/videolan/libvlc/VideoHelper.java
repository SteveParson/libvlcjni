package org.videolan.libvlc;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.util.Log;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import org.videolan.R;
import org.videolan.libvlc.interfaces.IMedia;
import org.videolan.libvlc.interfaces.IVLCVout;
import org.videolan.libvlc.util.AndroidUtil;
import org.videolan.libvlc.util.DisplayManager;
import org.videolan.libvlc.util.VLCVideoLayout;

class VideoHelper implements IVLCVout.OnNewVideoLayoutListener {
    private static final String TAG = "LibVLC/VideoHelper";

    private MediaPlayer.ScaleType mCurrentScaleType = MediaPlayer.ScaleType.SURFACE_BEST_FIT;

    private float mCustomScale;
    private boolean mCurrentScaleCustom = false;

    private int mVideoHeight = 0;
    private int mVideoWidth = 0;
    private int mVideoVisibleHeight = 0;
    private int mVideoVisibleWidth = 0;
    private int mVideoSarNum = 0;
    private int mVideoSarDen = 0;

    private FrameLayout mVideoSurfaceFrame;
    private SurfaceView mVideoSurface = null;
    private SurfaceView mSubtitlesSurface = null;
    private TextureView mVideoTexture = null;

    private final Handler mHandler = new Handler();
    private View.OnLayoutChangeListener mOnLayoutChangeListener = null;
    private DisplayManager mDisplayManager;

    private org.videolan.libvlc.MediaPlayer mMediaPlayer;

    VideoHelper(MediaPlayer player, VLCVideoLayout surfaceFrame, DisplayManager dm, boolean subtitles, boolean textureView) {
        init(player, surfaceFrame, dm, subtitles, !textureView);
    }

    private void init(MediaPlayer player, VLCVideoLayout surfaceFrame, DisplayManager dm, boolean subtitles, boolean useSurfaceView) {
        mMediaPlayer = player;
        mDisplayManager = dm;
        final boolean isPrimary = mDisplayManager == null || mDisplayManager.isPrimary();
        if (isPrimary) {
            mVideoSurfaceFrame = surfaceFrame.findViewById(R.id.player_surface_frame);
            if (useSurfaceView) {
                ViewStub stub = mVideoSurfaceFrame.findViewById(R.id.surface_stub);
                mVideoSurface = stub != null ? (SurfaceView) stub.inflate() : (SurfaceView) mVideoSurfaceFrame.findViewById(R.id.surface_video);
                if (subtitles) {
                    stub = surfaceFrame.findViewById(R.id.subtitles_surface_stub);
                    mSubtitlesSurface = stub != null ? (SurfaceView) stub.inflate() : (SurfaceView) surfaceFrame.findViewById(R.id.surface_subtitles);
                    mSubtitlesSurface.setZOrderMediaOverlay(true);
                    mSubtitlesSurface.getHolder().setFormat(PixelFormat.TRANSLUCENT);
                }
            } else {
                final ViewStub stub = mVideoSurfaceFrame.findViewById(R.id.texture_stub);
                mVideoTexture = stub != null ? (TextureView) stub.inflate() : (TextureView) mVideoSurfaceFrame.findViewById(R.id.texture_video);;
            }
        } else if (mDisplayManager.getPresentation() != null){
            mVideoSurfaceFrame = mDisplayManager.getPresentation().getSurfaceFrame();
            mVideoSurface = mDisplayManager.getPresentation().getSurfaceView();
            mSubtitlesSurface = mDisplayManager.getPresentation().getSubtitlesSurfaceView();
        }
    }

    void release() {
        if (mMediaPlayer.getVLCVout().areViewsAttached()) detachViews();
        mMediaPlayer = null;
        mVideoSurfaceFrame = null;
        mHandler.removeCallbacks(null);
        mVideoSurface = null;
        mSubtitlesSurface = null;
        mVideoTexture = null;
    }

    void attachViews() {
        if (mVideoSurface == null && mVideoTexture == null) return;
        final IVLCVout vlcVout = mMediaPlayer.getVLCVout();
        if (mVideoSurface != null) {
            vlcVout.setVideoView(mVideoSurface);
            if (mSubtitlesSurface != null)
                vlcVout.setSubtitlesView(mSubtitlesSurface);
        } else if (mVideoTexture != null)
            vlcVout.setVideoView(mVideoTexture);
        else return;
        vlcVout.attachViews(this);

        if (mOnLayoutChangeListener == null) {
            mOnLayoutChangeListener = new View.OnLayoutChangeListener() {
                private final Runnable runnable = new Runnable() {
                    @Override
                    public void run() {
                        if (mVideoSurfaceFrame != null && mOnLayoutChangeListener != null) updateVideoSurfaces();
                    }
                };
                @Override
                public void onLayoutChange(View v, int left, int top, int right,
                                           int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    if (left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom) {
                        mHandler.removeCallbacks(runnable);
                        mHandler.post(runnable);
                    }
                }
            };
        }
        mVideoSurfaceFrame.addOnLayoutChangeListener(mOnLayoutChangeListener);
        mMediaPlayer.setVideoTrackEnabled(true);
    }

    void detachViews() {
        if (mOnLayoutChangeListener != null && mVideoSurfaceFrame != null) {
            mVideoSurfaceFrame.removeOnLayoutChangeListener(mOnLayoutChangeListener);
            mOnLayoutChangeListener = null;
        }
        mMediaPlayer.setVideoTrackEnabled(false);
        mMediaPlayer.getVLCVout().detachViews();
    }

    private void changeMediaPlayerLayout(int displayW, int displayH) {
        if (mMediaPlayer.isReleased()) return;

        if (mCurrentScaleCustom) {
            mMediaPlayer.setAspectRatio(null);
            mMediaPlayer.setNativeScale(mCustomScale);
            return;
        }

        /* Change the video placement using the MediaPlayer API */
        switch (mCurrentScaleType) {
            case SURFACE_BEST_FIT:
                mMediaPlayer.setAspectRatio(null);
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_FIT_SCREEN:
            case SURFACE_FILL: {
                IMedia.VideoTrack vtrack = (IMedia.VideoTrack) mMediaPlayer.getSelectedTrack(Media.Track.Type.Video);
                if (vtrack == null)
                    return;
                final boolean videoSwapped = vtrack.orientation == IMedia.VideoTrack.Orientation.LeftBottom
                        || vtrack.orientation == IMedia.VideoTrack.Orientation.RightTop;
                if (mCurrentScaleType == MediaPlayer.ScaleType.SURFACE_FIT_SCREEN) {
                    int videoW = vtrack.width;
                    int videoH = vtrack.height;

                    if (videoSwapped) {
                        int swap = videoW;
                        videoW = videoH;
                        videoH = swap;
                    }
                    if (vtrack.sarNum != vtrack.sarDen)
                        videoW = videoW * vtrack.sarNum / vtrack.sarDen;

                    float ar = videoW / (float) videoH;
                    float dar = displayW / (float) displayH;

                    float scale;
                    if (dar >= ar)
                        scale = displayW / (float) videoW; /* horizontal */
                    else
                        scale = displayH / (float) videoH; /* vertical */
                    mMediaPlayer.setNativeScale(scale);
                    mMediaPlayer.setAspectRatio(null);
                } else {
                    mMediaPlayer.setNativeScale(0);
                    mMediaPlayer.setAspectRatio(!videoSwapped ? ""+displayW+":"+displayH
                            : ""+displayH+":"+displayW);
                }
                break;
            }
            case SURFACE_16_9:
                mMediaPlayer.setAspectRatio("16:9");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_16_10:
                mMediaPlayer.setAspectRatio("16:10");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_221_1:
                mMediaPlayer.setAspectRatio("221:100");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_235_1:
                mMediaPlayer.setAspectRatio("235:100");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_239_1:
                mMediaPlayer.setAspectRatio("239:100");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_5_4:
                mMediaPlayer.setAspectRatio("5:4");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_4_3:
                mMediaPlayer.setAspectRatio("4:3");
                mMediaPlayer.setNativeScale(0);
                break;
            case SURFACE_ORIGINAL:
                mMediaPlayer.setAspectRatio(null);
                mMediaPlayer.setNativeScale(1);
                break;
        }
    }

    void updateVideoSurfaces() {
        if (mMediaPlayer == null || mMediaPlayer.isReleased() || !mMediaPlayer.getVLCVout().areViewsAttached()) return;
        final boolean isPrimary = mDisplayManager == null || mDisplayManager.isPrimary();
        final Activity activity = !isPrimary ? null : AndroidUtil.resolveActivity(mVideoSurfaceFrame.getContext());

        int sw;
        int sh;

        // get screen size
        if (activity != null) {
            sw = mVideoSurfaceFrame.getWidth();
            sh = mVideoSurfaceFrame.getHeight();
        } else if (mDisplayManager != null && mDisplayManager.getPresentation() != null && mDisplayManager.getPresentation().getWindow() != null) {
            sw = mDisplayManager.getPresentation().getWindow().getDecorView().getWidth();
            sh = mDisplayManager.getPresentation().getWindow().getDecorView().getHeight();
        } else return;

        // sanity check
        if (sw * sh == 0) {
            Log.e(TAG, "Invalid surface size");
            return;
        }

        mMediaPlayer.getVLCVout().setWindowSize(sw, sh);

        /* We will setup either the videoSurface or the videoTexture */
        View videoView = mVideoSurface;
        if (videoView == null)
            videoView = mVideoTexture;

        ViewGroup.LayoutParams lp = videoView.getLayoutParams();
        if (mVideoWidth * mVideoHeight == 0 || (AndroidUtil.isNougatOrLater && activity != null && activity.isInPictureInPictureMode())) {
            changeMediaPlayerLayout(sw, sh);
            /* Case of OpenGL vouts: handles the placement of the video using MediaPlayer API */
            lp.width  = ViewGroup.LayoutParams.MATCH_PARENT;
            lp.height = ViewGroup.LayoutParams.MATCH_PARENT;
            videoView.setLayoutParams(lp);
            lp = mVideoSurfaceFrame.getLayoutParams();
            lp.width  = ViewGroup.LayoutParams.MATCH_PARENT;
            lp.height = ViewGroup.LayoutParams.MATCH_PARENT;
            mVideoSurfaceFrame.setLayoutParams(lp);
            return;
        }

        if (lp.width == lp.height && lp.width == ViewGroup.LayoutParams.MATCH_PARENT) {
            /* We handle the placement of the video using Android View LayoutParams */
            mMediaPlayer.setAspectRatio(null);
            mMediaPlayer.setNativeScale(0);
        }

        double dw = sw, dh = sh;
        boolean consideredPortrait = mVideoSurfaceFrame.getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        if (mMediaPlayer.useOrientationFromBounds()) consideredPortrait = sh > sw;
        final boolean isPortrait = isPrimary && consideredPortrait;

        if (sw > sh && isPortrait || sw < sh && !isPortrait) {
            dw = sh;
            dh = sw;
        }

        // compute the aspect ratio
        double ar, vw;
        if (mVideoSarDen == mVideoSarNum) {
            /* No indication about the density, assuming 1:1 */
            vw = mVideoVisibleWidth;
            ar = (double)mVideoVisibleWidth / (double)mVideoVisibleHeight;
        } else {
            /* Use the specified aspect ratio */
            vw = mVideoVisibleWidth * (double)mVideoSarNum / mVideoSarDen;
            ar = vw / mVideoVisibleHeight;
        }

        // compute the display aspect ratio
        double dar = dw / dh;

        MediaPlayer.ScaleType scaleType = mCurrentScaleType;
        if (mCurrentScaleCustom) {
            dh *= mCustomScale;
            dw *= mCustomScale;
            scaleType = MediaPlayer.ScaleType.SURFACE_BEST_FIT;
        }

        switch (scaleType) {
            case SURFACE_BEST_FIT:
                if (dar < ar)
                    dh = dw / ar;
                else
                    dw = dh * ar;
                break;
            case SURFACE_FIT_SCREEN:
                if (dar >= ar)
                    dh = dw / ar; /* horizontal */
                else
                    dw = dh * ar; /* vertical */
                break;
            case SURFACE_FILL:
                break;
            case SURFACE_ORIGINAL:
                dh = mVideoVisibleHeight;
                dw = vw;
                break;
            default:
                ar = mCurrentScaleType.getRatio();
                if (dar < ar)
                    dh = dw / ar;
                else
                    dw = dh * ar;
                break;
        }

        // set display size
        lp.width  = (int) Math.ceil(dw * mVideoWidth / mVideoVisibleWidth);
        lp.height = (int) Math.ceil(dh * mVideoHeight / mVideoVisibleHeight);
        videoView.setLayoutParams(lp);

        videoView.invalidate();
    }

    @Override
    public void onNewVideoLayout(IVLCVout vlcVout, int width, int height, int visibleWidth, int visibleHeight, int sarNum, int sarDen) {
        if (width == 0 && height == 0 && visibleWidth == 0 && visibleHeight == 0 && sarNum == 0 && sarDen == 0) {
            mVideoWidth = mVideoHeight = mVideoVisibleWidth = mVideoVisibleHeight = 0;
            mVideoSarNum = mVideoSarDen = 0;
        } else {
            if (width != 0 && height != 0) {
                mVideoWidth = width;
                mVideoHeight = height;
            }
            if (visibleWidth != 0 && visibleHeight != 0) {
                mVideoVisibleWidth = visibleWidth;
                mVideoVisibleHeight = visibleHeight;
            }
            if (sarNum != 0 && sarDen != 0) {
                mVideoSarNum = sarNum;
                mVideoSarDen = sarDen;
            }
        }
        updateVideoSurfaces();
    }

    void setVideoScale(MediaPlayer.ScaleType type) {
        mCurrentScaleType = type;
        mCurrentScaleCustom = false;
        updateVideoSurfaces();
    }

    void setCustomScale(float scale) {
        mCustomScale = scale;
        mCurrentScaleCustom = true;
        updateVideoSurfaces();
    }

    MediaPlayer.ScaleType getVideoScale() {
        return mCurrentScaleType;
    }
}
