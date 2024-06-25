/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "GStreamerVideo.h"
#include "../Database/Configuration.h"
#include "../Graphics/Component/Image.h"
#include "../Graphics/ViewInfo.h"
#include "../SDL.h"
#include "../Utility/Log.h"
#include "../Utility/Utils.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <gst/gstdebugutils.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

bool GStreamerVideo::initialized_ = false;

GStreamerVideo::GStreamerVideo(int monitor)

    : monitor_(monitor)

{
}

GStreamerVideo::~GStreamerVideo()
{
    GStreamerVideo::stop();
}

void GStreamerVideo::setNumLoops(int n)
{
    if (n > 0)
        numLoops_ = n;
}

SDL_Texture *GStreamerVideo::getTexture() const
{
    return texture_;
}

bool GStreamerVideo::initialize()
{
    if (initialized_)
    {
        initialized_ = true;
        paused_ = false;
        return true;
    }

    if (!gst_is_initialized())
    {
        LOG_DEBUG("GStreamer", "Initializing in instance");
        gst_init(nullptr, nullptr);
        std::string path = Utils::combinePath(Configuration::absolutePath, "retrofe");
#ifdef WIN32
        GstRegistry *registry = gst_registry_get();
        gst_registry_scan_path(registry, path.c_str());
#endif
    }

    initialized_ = true;
    paused_ = false;

    return true;
}

bool GStreamerVideo::deInitialize()
{
    gst_deinit();
    initialized_ = false;
    paused_ = false;
    return true;
}

bool GStreamerVideo::stop()
{
    if (!initialized_)
    {
        return false;
    }

    gst_app_sink_set_callbacks(GST_APP_SINK(videoSink_), NULL, NULL, NULL);
    frameReady_ = true;

    // Initiate the transition of playbin to GST_STATE_NULL without waiting
    if (playbin_)
    {
        gst_element_set_state(playbin_, GST_STATE_NULL);

        // Optionally perform a quick, non-blocking state check
        GstStateChangeReturn ret = gst_element_get_state(playbin_, nullptr, nullptr, 0);
        if (ret != GST_STATE_CHANGE_SUCCESS && ret != GST_STATE_CHANGE_ASYNC)
        {
            LOG_ERROR("Video", "Unexpected state change result when stopping playback");
        }
    }

    // Release SDL Texture
    if (texture_)
    {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    // Unref the video buffer
    if (videoBuffer_)
    {
        gst_clear_buffer(&videoBuffer_);
    }

    // Free GStreamer elements and related resources
    if (playbin_)
    {
        gst_object_unref(GST_OBJECT(playbin_));
        playbin_ = nullptr;
    }

    // Reset remaining pointers and variables to ensure the object is in a clean state.
    videoBus_ = nullptr;
    playbin_ = nullptr;
    videoBin_ = nullptr;
    capsFilter_ = nullptr;
    videoSink_ = nullptr;
    videoBuffer_ = nullptr;

    isPlaying_ = false;

    return true;
}

bool GStreamerVideo::play(const std::string &file)
{
    playCount_ = 0;

    if (!initialized_)
        return false;

#if defined(WIN32)
    enablePlugin("directsoundsink");
    if (!Configuration::HardwareVideoAccel)
    {
        // enablePlugin("openh264dec");
        disablePlugin("d3d11h264dec");
        disablePlugin("d3d11h265dec");
    }
#elif defined(__APPLE__)
    if (Configuration::HardwareVideoAccel)
    {
        enablePlugin("vah264dec");
        enablePlugin("vah265dec");
    }
#else
    if (Configuration::HardwareVideoAccel)
    {
        enablePlugin("vah264dec");
        enablePlugin("vah265dec");
    }
    if (!Configuration::HardwareVideoAccel)
    {
        disablePlugin("vah264dec");
        disablePlugin("vah265dec");
        enablePlugin("avdec_h264");
        enablePlugin("avdec_h265");
    }
#endif

    currentFile_ = file;

    if (!initializeGstElements(file))
        return false;

    // Start playing
    if (GstStateChangeReturn playState = gst_element_set_state(GST_ELEMENT(playbin_), GST_STATE_PLAYING);
        playState != GST_STATE_CHANGE_ASYNC)
    {
        isPlaying_ = false;
        LOG_ERROR("Video", "Unable to set the pipeline to the playing state.");
        stop();
        return false;
    }

    isPlaying_ = true;

    // Set the volume to zero and mute the video
    gst_stream_volume_set_volume(GST_STREAM_VOLUME(playbin_), GST_STREAM_VOLUME_FORMAT_LINEAR, 0.0);
    gst_stream_volume_set_mute(GST_STREAM_VOLUME(playbin_), true);

    if (Configuration::debugDotEnabled)
    {
        // Environment variable is set, proceed with dot file generation

        GstState state;
        GstState pending;
        // Wait up to 5 seconds for the state change to complete
        GstClockTime timeout = 5 * GST_SECOND; // Define your timeout
        GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(playbin_), &state, &pending, timeout);

        if (ret == GST_STATE_CHANGE_SUCCESS && state == GST_STATE_PLAYING)
        {
            // The pipeline is in the playing state, proceed with dot file generation

            // Generate dot file for playbin_
            std::string playbinDotFileName = generateDotFileName("playbin", currentFile_);
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(playbin_), GST_DEBUG_GRAPH_SHOW_ALL, playbinDotFileName.c_str());

            // Generate dot file for videoBin_
            std::string videoBinDotFileName = generateDotFileName("videobin", currentFile_);
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(videoBin_), GST_DEBUG_GRAPH_SHOW_ALL, videoBinDotFileName.c_str());
        }
    }

    return true;
}

bool GStreamerVideo::initializeGstElements(const std::string &file)
{
    gchar *uriFile = gst_filename_to_uri(file.c_str(), nullptr);

    if (!uriFile)
        return false;

    playbin_ = gst_element_factory_make("playbin3", "player");
    videoBin_ = gst_bin_new("SinkBin");
    videoSink_ = gst_element_factory_make("appsink", "video_sink");
    capsFilter_ = gst_element_factory_make("capsfilter", "caps_filter");
    GstCaps *videoConvertCaps;
    if (Configuration::HardwareVideoAccel)
    {
        videoConvertCaps = gst_caps_from_string("video/x-raw,format=(string)NV12,pixel-aspect-ratio=(fraction)1/1");
    }
    else
    {
        videoConvertCaps = gst_caps_from_string("video/x-raw,format=(string)I420,pixel-aspect-ratio=(fraction)1/1");
    }

    if (!playbin_ || !videoSink_ || !capsFilter_)
    {
        LOG_DEBUG("Video", "Could not create elements");
        return false;
    }

    g_object_set(G_OBJECT(capsFilter_), "caps", videoConvertCaps, nullptr);
    gst_caps_unref(videoConvertCaps);
    videoConvertCaps = nullptr;

    gst_bin_add_many(GST_BIN(videoBin_), capsFilter_, videoSink_, nullptr);

    // Directly link capsFilter to videoSink
    if (!gst_element_link(capsFilter_, videoSink_))
    {
        LOG_DEBUG("Video", "Could not link video processing elements");
        return false;
    }

    GstPad *sinkPad = nullptr;
    sinkPad = gst_element_get_static_pad(capsFilter_, "sink");

    GstPad *ghostPad = gst_ghost_pad_new("sink", sinkPad);
    gst_element_add_pad(videoBin_, ghostPad);
    gst_object_unref(sinkPad);

    // Set properties of playbin and videoSink
    const guint PLAYBIN_FLAGS = 0x00000001 | 0x00000002;
    g_object_set(G_OBJECT(playbin_), "uri", uriFile, "video-sink", videoBin_, "instant-uri", TRUE, "flags",
                 PLAYBIN_FLAGS, nullptr);
    g_free(uriFile);

    GstPad *pad = gst_element_get_static_pad(videoSink_, "sink");
    if (pad)
    {
        padProbeId_ = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, padProbeCallback, this, nullptr);
        gst_object_unref(pad);
    }

    // Configure the appsink
    gst_app_sink_set_emit_signals(GST_APP_SINK(videoSink_),
                                  false); // Ensure signals are not emitted

    g_object_set(GST_APP_SINK(videoSink_), "emit-signals", FALSE, "sync", TRUE, "enable-last-sample", TRUE,
        "wait-on-eos", FALSE, "max-buffers", 30, NULL);
    gst_app_sink_set_drop(GST_APP_SINK(videoSink_), true);
    GstAppSinkCallbacks callbacks{NULL, GStreamerVideo::new_buffer, GStreamerVideo::new_buffer, NULL, 0};
    gst_app_sink_set_callbacks(GST_APP_SINK(videoSink_), &callbacks, this, NULL);

    elementSetupHandlerId_ = g_signal_connect(playbin_, "element-setup", G_CALLBACK(elementSetupCallback), this);
    videoBus_ = gst_pipeline_get_bus(GST_PIPELINE(playbin_));
    gst_object_unref(videoBus_);

    return true;
}

GstPadProbeReturn GStreamerVideo::padProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    auto *video = static_cast<GStreamerVideo *>(user_data);

    auto *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS)
    {
        GstCaps *caps = nullptr;
        gst_event_parse_caps(event, &caps);
        if (caps)
        {
                GstStructure* s = gst_caps_get_structure(caps, 0);
                gst_structure_get_int(s, "width", &video->width_);
                gst_structure_get_int(s, "height", &video->height_);
                gst_pad_remove_probe(pad, video->padProbeId_);
            
            gst_caps_unref(caps);
        }
    }
    return GST_PAD_PROBE_OK;
}

void GStreamerVideo::elementSetupCallback([[maybe_unused]] GstElement const *playbin, GstElement *element,
                                          [[maybe_unused]] GStreamerVideo const *video)
{

    gchar *elementName = gst_element_get_name(element);
    if (!Configuration::HardwareVideoAccel)
    {
        if (g_str_has_prefix(elementName, "avdec_h26"))
        {
            // Modify the properties of the avdec_h265 element here
            g_object_set(G_OBJECT(element), "thread-type", Configuration::AvdecThreadType, "max-threads",
                         Configuration::AvdecMaxThreads, "direct-rendering", false, nullptr);
        }
    }
#ifdef WIN32
    if (strstr(elementName, "wasapi2") != nullptr)
    {
        g_object_set(G_OBJECT(element), "low-latency", TRUE, nullptr);
    }
#endif
    g_free(elementName);
}

GstFlowReturn GStreamerVideo::new_buffer(GstAppSink* app_sink, gpointer userdata)
{
    GStreamerVideo* video = static_cast<GStreamerVideo*>(userdata);
    GstFlowReturn ret = GST_FLOW_OK;

    // Lock the mutex to ensure thread safety.
    SDL_LockMutex(SDL::getMutex());

    if (video && video->isPlaying_ && !video->frameReady_)
    {
        // Get the last sample (non-blocking).
        GstSample* sample = nullptr;
        g_object_get(app_sink, "last-sample", &sample, NULL);

        if (sample != nullptr)
        {
            // Retrieve caps and set width/height if not yet set.
            if (!video->width_ || !video->height_)
            {
                GstCaps* caps = gst_sample_get_caps(sample);
                if (caps)
                {
                    GstStructure* s = gst_caps_get_structure(caps, 0);
                    gst_structure_get_int(s, "width", &video->width_);
                    gst_structure_get_int(s, "height", &video->height_);
                }
            }
            if (video->height_ && video->width_)
            {
                // Get the buffer from the sample.
                GstBuffer* buffer = gst_sample_get_buffer(sample);
                if (!buffer)
                {
                    gst_sample_unref(sample);
                    SDL_UnlockMutex(SDL::getMutex());
                    return GST_FLOW_OK;
                }

                // Clear the existing videoBuffer_ if it exists.
                if (video->videoBuffer_)
                {
                    gst_clear_buffer(&video->videoBuffer_);
                }

                // Make a copy of the incoming buffer and set it as the new videoBuffer_.
                video->videoBuffer_ = gst_buffer_copy(buffer);
                //gst_clear_buffer(&buffer);  // Unref the buffer after copying
                video->frameReady_ = true;
            }

            // Cleanup stack of samples (non-blocking).
            while (sample != nullptr)
            {
                gst_sample_unref(sample);
                sample = gst_app_sink_try_pull_sample(app_sink, 0);
            }
        }
        else
        {
            ret = GST_FLOW_FLUSHING;
        }
    }

    SDL_UnlockMutex(SDL::getMutex());
    return ret;
}

void GStreamerVideo::update(float /* dt */)
{

    if (!playbin_ || !videoBuffer_)
    {
        return;
    }

    SDL_LockMutex(SDL::getMutex());

    if (!texture_ && width_ != 0 && height_ !=0)
    {
        if (Configuration::HardwareVideoAccel)
        {
            texture_ = SDL_CreateTexture(SDL::getRenderer(monitor_), SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING,
                                         width_, height_);
        }
        else
        {
            texture_ = SDL_CreateTexture(SDL::getRenderer(monitor_), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                         width_, height_);
        }
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    }

    if (videoBuffer_)
    {
        GstVideoMeta* meta = gst_buffer_get_video_meta(videoBuffer_);

        // Check if hardware video acceleration is enabled
        if (Configuration::HardwareVideoAccel)
        {
            // NV12 texture update
            if (!meta)
            {
                void* pixels;
                int pitch;
                unsigned int ybytes = width_ * height_;
                unsigned int uvbytes = width_ * height_ / 2;
                gsize bufSize = gst_buffer_get_size(videoBuffer_);

                if (bufSize == (ybytes + uvbytes))
                {
                    SDL_LockTexture(texture_, NULL, &pixels, &pitch);
                    gst_buffer_extract(videoBuffer_, 0, pixels, ybytes + uvbytes);
                    SDL_UnlockTexture(texture_);
                }
                else
                {
                    GstMapInfo bufInfo;
                    unsigned int y_stride, uv_stride;
                    const Uint8* y_plane, * uv_plane;

                    y_stride = GST_ROUND_UP_4(width_);
                    uv_stride = GST_ROUND_UP_4(y_stride / 2);

                    gst_buffer_map(videoBuffer_, &bufInfo, GST_MAP_READ);
                    y_plane = bufInfo.data;
                    uv_plane = y_plane + (height_ * y_stride);
                    SDL_UpdateNVTexture(texture_, NULL,
                        (const Uint8*)y_plane, y_stride,
                        (const Uint8*)uv_plane, uv_stride);
                    gst_buffer_unmap(videoBuffer_, &bufInfo);
                }
            }
            else
            {
                GstMapInfo y_info, uv_info;
                void* y_plane, * uv_plane;
                int y_stride, uv_stride;

                gst_video_meta_map(meta, 0, &y_info, &y_plane, &y_stride, GST_MAP_READ);
                gst_video_meta_map(meta, 1, &uv_info, &uv_plane, &uv_stride, GST_MAP_READ);
                SDL_UpdateNVTexture(texture_, NULL,
                    (const Uint8*)y_plane, y_stride,
                    (const Uint8*)uv_plane, uv_stride);
                gst_video_meta_unmap(meta, 0, &y_info);
                gst_video_meta_unmap(meta, 1, &uv_info);
            }
        }
        else
        {
            // YUV texture update
            if (!meta)
            {
                void* pixels;
                int pitch;
                unsigned int vbytes = width_ * height_;
                vbytes += (vbytes / 2);
                gsize bufSize = gst_buffer_get_size(videoBuffer_);

                if (bufSize == vbytes)
                {
                    SDL_LockTexture(texture_, NULL, &pixels, &pitch);
                    gst_buffer_extract(videoBuffer_, 0, pixels, vbytes);
                    SDL_UnlockTexture(texture_);
                }
                else
                {
                    GstMapInfo bufInfo;
                    unsigned int y_stride, u_stride, v_stride;
                    const Uint8* y_plane, * u_plane, * v_plane;

                    y_stride = GST_ROUND_UP_4(width_);
                    u_stride = v_stride = GST_ROUND_UP_4(y_stride / 2);

                    gst_buffer_map(videoBuffer_, &bufInfo, GST_MAP_READ);
                    y_plane = bufInfo.data;
                    u_plane = y_plane + (height_ * y_stride);
                    v_plane = u_plane + ((height_ / 2) * u_stride);
                    SDL_UpdateYUVTexture(texture_, NULL,
                        (const Uint8*)y_plane, y_stride,
                        (const Uint8*)u_plane, u_stride,
                        (const Uint8*)v_plane, v_stride);
                    gst_buffer_unmap(videoBuffer_, &bufInfo);
                }
            }
            else
            {
                GstMapInfo y_info, u_info, v_info;
                void* y_plane, * u_plane, * v_plane;
                int y_stride, u_stride, v_stride;

                gst_video_meta_map(meta, 0, &y_info, &y_plane, &y_stride, GST_MAP_READ);
                gst_video_meta_map(meta, 1, &u_info, &u_plane, &u_stride, GST_MAP_READ);
                gst_video_meta_map(meta, 2, &v_info, &v_plane, &v_stride, GST_MAP_READ);
                SDL_UpdateYUVTexture(texture_, NULL,
                    (const Uint8*)y_plane, y_stride,
                    (const Uint8*)u_plane, u_stride,
                    (const Uint8*)v_plane, v_stride);
                gst_video_meta_unmap(meta, 0, &y_info);
                gst_video_meta_unmap(meta, 1, &u_info);
                gst_video_meta_unmap(meta, 2, &v_info);
            }
        }

        gst_clear_buffer(&videoBuffer_);
    }

    SDL_UnlockMutex(SDL::getMutex());
}

void GStreamerVideo::loopHandler()
{
    if (videoBus_)
    {
        GstMessage *msg = gst_bus_pop_filtered(videoBus_, GST_MESSAGE_EOS);
        if (msg)
        {
            playCount_++;

            // If the number of loops is 0 or greater than the current playCount_, seek the playback to the beginning.
            if (!numLoops_ || numLoops_ > playCount_)
            {
                gst_element_seek(playbin_, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
                                 GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
            }
            else
            {
                stop();
            }
            gst_message_unref(msg);
        }
    }
}

void GStreamerVideo::volumeUpdate()
{
    bool shouldMute = false;
    double targetVolume = 0.0;
    if (bool muteVideo = Configuration::MuteVideo; muteVideo)
    {
        shouldMute = true;
    }
    else
    {
        if (volume_ > 1.0)
            volume_ = 1.0;
        if (currentVolume_ > volume_ || currentVolume_ + 0.005 >= volume_)
            currentVolume_ = volume_;
        else
            currentVolume_ += 0.005;
        targetVolume = currentVolume_;
        if (currentVolume_ < 0.1)
            shouldMute = true;
    }

    // Only set the volume if it has changed since the last call.
    if (targetVolume != lastSetVolume_)
    {
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(playbin_), GST_STREAM_VOLUME_FORMAT_LINEAR, targetVolume);
        lastSetVolume_ = targetVolume;
    }
    // Only set the mute state if it has changed since the last call.
    if (shouldMute != lastSetMuteState_)
    {
        gst_stream_volume_set_mute(GST_STREAM_VOLUME(playbin_), shouldMute);
        lastSetMuteState_ = shouldMute;
    }
}

int GStreamerVideo::getHeight()
{
    return height_;
}

int GStreamerVideo::getWidth()
{
    return width_;
}

void GStreamerVideo::draw()
{
    frameReady_ = false;
}

bool GStreamerVideo::isPlaying()
{
    return isPlaying_;
}

void GStreamerVideo::setVolume(float volume)
{
    volume_ = volume;
}

void GStreamerVideo::skipForward()
{

    if (!isPlaying_)
        return;

    gint64 current;
    gint64 duration;

    if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &current))
        return;

    if (!gst_element_query_duration(playbin_, GST_FORMAT_TIME, &duration))
        return;

    current += 60 * GST_SECOND;
    if (current > duration)
        current = duration - 1;
    gst_element_seek_simple(playbin_, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            current);
}

void GStreamerVideo::skipBackward()
{

    if (!isPlaying_)
        return;

    gint64 current;

    if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &current))
        return;

    if (current > 60 * GST_SECOND)
        current -= 60 * GST_SECOND;
    else
        current = 0;
    gst_element_seek_simple(playbin_, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            current);
}

void GStreamerVideo::skipForwardp()
{

    if (!isPlaying_)
        return;

    gint64 current;
    gint64 duration;

    if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &current))
        return;

    if (!gst_element_query_duration(playbin_, GST_FORMAT_TIME, &duration))
        return;

    current += duration / 20;
    if (current > duration)
        current = duration - 1;
    gst_element_seek_simple(playbin_, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            current);
}

void GStreamerVideo::skipBackwardp()
{

    if (!isPlaying_)
        return;

    gint64 current;
    gint64 duration;

    if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &current))
        return;

    if (!gst_element_query_duration(playbin_, GST_FORMAT_TIME, &duration))
        return;

    if (current > duration / 20)
        current -= duration / 20;
    else
        current = 0;
    gst_element_seek_simple(playbin_, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            current);
}

void GStreamerVideo::pause()
{
    paused_ = !paused_;
    if (paused_)
    {
        gst_element_set_state(GST_ELEMENT(playbin_), GST_STATE_PAUSED);
    }
    else
        gst_element_set_state(GST_ELEMENT(playbin_), GST_STATE_PLAYING);
}

void GStreamerVideo::restart()
{

    if (!isPlaying_)
        return;

    gst_element_seek_simple(playbin_, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH), 0);
}

unsigned long long GStreamerVideo::getCurrent()
{
    gint64 ret = 0;
    if (!gst_element_query_position(playbin_, GST_FORMAT_TIME, &ret) || !isPlaying_)
        ret = 0;
    return (unsigned long long)ret;
}

unsigned long long GStreamerVideo::getDuration()
{
    gint64 ret = 0;
    if (!gst_element_query_duration(playbin_, GST_FORMAT_TIME, &ret) || !isPlaying_)
        ret = 0;
    return (unsigned long long)ret;
}

bool GStreamerVideo::isPaused()
{
    return paused_;
}

bool GStreamerVideo::getFrameReady()
{
    return frameReady_;
}

std::string GStreamerVideo::generateDotFileName(const std::string &prefix, const std::string &videoFilePath)
{
    std::string videoFileName = Utils::getFileName(videoFilePath);

    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;

    std::stringstream ss;
    ss << prefix << "_" << videoFileName << "_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S_")
       << std::setfill('0') << std::setw(6) << microseconds.count();

    return ss.str();
}

void GStreamerVideo::enablePlugin(const std::string &pluginName)
{
    GstElementFactory *factory = gst_element_factory_find(pluginName.c_str());
    if (factory)
    {
        // Sets the plugin rank to PRIMARY + 1 to prioritize its use
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(factory), GST_RANK_PRIMARY + 1);
        gst_object_unref(factory);
    }
}

void GStreamerVideo::disablePlugin(const std::string &pluginName)
{
    GstElementFactory *factory = gst_element_factory_find(pluginName.c_str());
    if (factory)
    {
        // Sets the plugin rank to GST_RANK_NONE to disable its use
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(factory), GST_RANK_NONE);
        gst_object_unref(factory);
    }
}
