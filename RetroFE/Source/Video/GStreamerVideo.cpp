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
bool GStreamerVideo::pluginsInitialized_ = false;

GStreamerVideo::GStreamerVideo(int monitor)

    : monitor_(monitor)

{
}

GStreamerVideo::~GStreamerVideo()
{
    GStreamerVideo::stop();
}

void GStreamerVideo::initializePlugins()
{
    if (!pluginsInitialized_)
    {
        pluginsInitialized_ = true;

#if defined(WIN32)
        enablePlugin("directsoundsink");
        disablePlugin("mfdeviceprovider");
        if (!Configuration::HardwareVideoAccel)
        {
            // enablePlugin("openh264dec");
            disablePlugin("d3d11h264dec");
            disablePlugin("d3d11h265dec");
            disablePlugin("GstNvH264Dec");
            enablePlugin("avdec_h264");
            enablePlugin("avdec_h265");
        }
        else
        {
            enablePlugin("d3d11h264dec");

            // enablePlugin("qsvh264dec");
        }
#elif defined(__APPLE__)
        // if (Configuration::HardwareVideoAccel) {
        //     enablePlugin("vah264dec");
        //     enablePlugin("vah265dec");
        // }
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
    }
}


void GStreamerVideo::setNumLoops(int n)
{
    if (n > 0)
        numLoops_ = n;
}

SDL_Texture* GStreamerVideo::getTexture() const
{
    SDL_LockMutex(SDL::getMutex());
    SDL_Texture* texture = texture_;
    SDL_UnlockMutex(SDL::getMutex());
    return texture;
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
    initializePlugins();
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

    if (playbin_)
    {
        // Set the pipeline state to NULL to clean up
        gst_element_set_state(playbin_, GST_STATE_NULL);

        isPlaying_ = false;

        // Disconnect the signal handler for element setup if connected
        if (elementSetupHandlerId_ != 0)
        {
            g_signal_handler_disconnect(playbin_, elementSetupHandlerId_);
            elementSetupHandlerId_ = 0;
        }

        // Unreference the bus
        if (videoBus_)
        {
            gst_object_unref(videoBus_);
            videoBus_ = nullptr;
        }

        // Unreference and nullify the elements
        gst_object_unref(playbin_);
        playbin_ = nullptr;
        videoSink_ = nullptr;  // We don't unref videoSink_ because it's unrefed by playbin_

        LOG_DEBUG("Video", "GStreamer pipeline and elements cleaned up");
    }
    SDL_LockMutex(SDL::getMutex());
    // Release SDL Texture
    if (texture_)
    {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    SDL_UnlockMutex(SDL::getMutex());
    // Reset remaining pointers and variables to ensure the object is in a clean state.
    videoBus_ = nullptr;
    playbin_ = nullptr;
    videoSink_ = nullptr;


    return true;
}

bool GStreamerVideo::play(const std::string &file)
{
    playCount_ = 0;

    if (!initialized_)
        return false;
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
        }
    }

    return true;
}

bool GStreamerVideo::initializeGstElements(const std::string& file)
{
    gchar* uriFile = gst_filename_to_uri(file.c_str(), nullptr);

    if (!uriFile)
        return false;

    playbin_ = gst_element_factory_make("playbin", "playbin");
    videoSink_ = gst_element_factory_make("appsink", "appsink");

    if (!playbin_ || !videoSink_)
    {
        LOG_DEBUG("Video", "Could not create elements");
        if (uriFile)
            g_free(uriFile);
        return false;
    }

    GstCaps* videoConvertCaps = gst_caps_new_empty();
    if (Configuration::HardwareVideoAccel)
    {
        videoConvertCaps = gst_caps_from_string("video/x-raw,format=(string)NV12,pixel-aspect-ratio=(fraction)1/1");
        sdlFormat_ = SDL_PIXELFORMAT_NV12;
    }
    else
    {
        videoConvertCaps = gst_caps_from_string("video/x-raw,format=(string)I420,pixel-aspect-ratio=(fraction)1/1");
        sdlFormat_ = SDL_PIXELFORMAT_IYUV;
    }

    // Configure the appsink
    gst_app_sink_set_emit_signals(GST_APP_SINK(videoSink_), FALSE);
    g_object_set(GST_APP_SINK(videoSink_), "sync", TRUE, "enable-last-sample", TRUE,
        "wait-on-eos", FALSE, "max-buffers", 5, "caps", videoConvertCaps, nullptr);
    gst_app_sink_set_drop(GST_APP_SINK(videoSink_), true);
    gst_caps_unref(videoConvertCaps);

    // Set properties of playbin
    const guint PLAYBIN_FLAGS = 0x00000001 | 0x00000002;
    g_object_set(G_OBJECT(playbin_), "uri", uriFile, "video-sink", videoSink_, "flags", PLAYBIN_FLAGS, nullptr);
    g_free(uriFile);

    elementSetupHandlerId_ = g_signal_connect(playbin_, "element-setup", G_CALLBACK(elementSetupCallback), this);
    videoBus_ = gst_pipeline_get_bus(GST_PIPELINE(playbin_));
    gst_object_unref(videoBus_);

    return true;
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
   if (strstr(elementName, "vconv") != nullptr)
    {
        g_object_set(G_OBJECT(element), "use-converters", FALSE, nullptr);
    }
    g_free(elementName);
}


void GStreamerVideo::update(float /* dt */)
{

}

void GStreamerVideo::setVisibility(bool isVisible)
{
    if (isVisible != isVisible_)
    {
        isVisible_ = isVisible;

        if (!isVisible_)
        {
            // Set max-buffers to 1 to flush the appsink when the video is not visible
            g_object_set(GST_APP_SINK(videoSink_), "max-buffers", 1, nullptr);
        }
        else
        {
            // Reset max-buffers to 15 when the video becomes visible
            g_object_set(GST_APP_SINK(videoSink_), "max-buffers", 5, nullptr);
        }
    }
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
    if (!playbin_ || !videoSink_)
    {
        return;
    }

    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(videoSink_), 0);

    if (!sample)
    {
        return;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer)
    {
        gst_sample_unref(sample);
        return;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
    {
        gst_sample_unref(sample);
        return;
    }

    GstVideoInfo* videoInfo = gst_video_info_new();
    if (!gst_video_info_from_caps(videoInfo, caps))
    {
        gst_sample_unref(sample);
        gst_video_info_free(videoInfo); // Free the allocated memory for videoInfo
        return;
    }

    SDL_LockMutex(SDL::getMutex());

    if (!texture_ && videoInfo->width != 0 && videoInfo->height != 0)
    {
        texture_ = SDL_CreateTexture(SDL::getRenderer(monitor_), sdlFormat_, SDL_TEXTUREACCESS_STREAMING, videoInfo->width, videoInfo->height);
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    }

    if (texture_)
    {
        GstVideoFrame vframe(GST_VIDEO_FRAME_INIT);
        if (gst_video_frame_map(&vframe, videoInfo, buffer, (GstMapFlags)(GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF)))
        {
            if (sdlFormat_ == SDL_PIXELFORMAT_NV12)
            {
                if (SDL_UpdateNVTexture(texture_, nullptr,
                    static_cast<const Uint8*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0)),
                    GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0),
                    static_cast<const Uint8*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 1)),
                    GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 1)) != 0)
                {
                    LOG_ERROR("Video", "SDL_UpdateNVTexture failed: " + std::string(SDL_GetError()));
                }
            }
            else if (sdlFormat_ == SDL_PIXELFORMAT_IYUV)
            {
                if (SDL_UpdateYUVTexture(texture_, nullptr,
                    static_cast<const Uint8*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0)),
                    GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0),
                    static_cast<const Uint8*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 1)),
                    GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 1),
                    static_cast<const Uint8*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 2)),
                    GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 2)) != 0)
                {
                    LOG_ERROR("Video", "SDL_UpdateYUVTexture failed: " + std::string(SDL_GetError()));
                }
            }
            else
            {
                LOG_ERROR("Video", "Unsupported format or fallback handling required.");
            }

            gst_video_frame_unmap(&vframe);
        }
    }

    gst_sample_unref(sample);
    gst_video_info_free(videoInfo); // Free the allocated memory for videoInfo
    SDL_UnlockMutex(SDL::getMutex());
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
