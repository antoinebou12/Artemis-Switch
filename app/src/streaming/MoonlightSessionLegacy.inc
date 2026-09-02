#include "MoonlightSession.hpp"
#include "AVFrameHolder.hpp"
#include "FramePipelineTelemetry.hpp"
#include "GameStreamClient.hpp"
#include "InputManager.hpp"
#include "Settings.hpp"
#include "StreamDisconnectPolicy.hpp"
#include "StreamProfileStore.hpp"
#include "borealis.hpp"
#include "../features/host/CodecCapabilities.hpp"
#include <string.h>

#if defined(PLATFORM_IOS) || defined(PLATFORM_VISIONOS)
extern void getWindowSize(int* w, int* h);
#endif

using namespace brls;

int m_video_format;
// Atomic so decoder/audio/connection worker threads never observe a torn or
// half-written pointer while the UI thread tears the session down. The object
// may still be destroyed after clear(), so callbacks must not dereference only
// on this null-check alone; see ActiveSessionGuard for the full pinning model.
static std::atomic<MoonlightSession*> m_active_session = nullptr;
static MoonlightSessionDecoderAndRenderProvider* m_provider = nullptr;

static MoonlightSession* active_session_ptr() noexcept {
    return m_active_session.load(std::memory_order_acquire);
}

MoonlightSession* MoonlightSession::activeSession() {
    return active_session_ptr();
}

void MoonlightSession::set_provider(
    MoonlightSessionDecoderAndRenderProvider* provider) {
    m_provider = provider;
}

MoonlightSession::MoonlightSession(const std::string& address, int app_id,
                                   std::string appUuid) {
    m_address = address;
    m_app_id = app_id;
    m_app_uuid = std::move(appUuid);
    m_input_only = m_app_uuid == ApolloRemoteInputUuid;
    m_apollo_launch_options.appUuid = m_app_uuid;
    m_active_session.store(this, std::memory_order_release);

    m_video_decoder = m_provider->video_decoder();
    m_video_renderer = m_provider->video_renderer();
    m_audio_renderer = m_provider->audio_renderer();
}

MoonlightSession::~MoonlightSession() {
    if (m_video_decoder) {
        delete m_video_decoder;
    }

    if (m_video_renderer) {
        delete m_video_renderer;
    }

    if (m_audio_renderer) {
        delete m_audio_renderer;
    }

    m_active_session.store(nullptr, std::memory_order_release);
}

// MARK: Connection callbacks

static const char* stages[] = {"STAGE_NONE",
                               "STAGE_PLATFORM_INIT",
                               "STAGE_NAME_RESOLUTION",
                               "STAGE_RTSP_HANDSHAKE",
                               "STAGE_CONTROL_STREAM_INIT",
                               "STAGE_VIDEO_STREAM_INIT",
                               "STAGE_AUDIO_STREAM_INIT",
                               "STAGE_INPUT_STREAM_INIT",
                               "STAGE_CONTROL_STREAM_START",
                               "STAGE_VIDEO_STREAM_START",
                               "STAGE_AUDIO_STREAM_START",
                               "STAGE_INPUT_STREAM_START"};

void MoonlightSession::connection_stage_starting(int stage) {
    brls::Logger::info("MoonlightSession: Starting: {}", stages[stage]);
}

void MoonlightSession::connection_stage_complete(int stage) {
    brls::Logger::info("MoonlightSession: Complete: {}", stages[stage]);
}

void MoonlightSession::connection_stage_failed(int stage, int error_code) {
    brls::Logger::error("MoonlightSession: Failed: {} with error code: {}", stages[stage], error_code);
}

void MoonlightSession::connection_started() {
    brls::Logger::info("MoonlightSession: Connection started");
    if (!active_session_ptr())
        return;

    active_session_ptr()->m_stop_requested = false;
    active_session_ptr()->m_is_active = true;
    active_session_ptr()->m_restart_in_progress = false;
}

void MoonlightSession::connection_terminated(int error_code) {
    brls::Logger::info("MoonlightSession: Connection terminated with code: {}", error_code);

    if (!active_session_ptr())
        return;

    const auto action = artemis::streaming::connectionTerminatedAction(
        active_session_ptr()->m_stop_requested,
        active_session_ptr()->m_restart_in_progress.load(), error_code);

    // Never LiStopConnection / start() here. This callback runs on a Moonlight
    // worker thread; tearing down deko3d there orange-screens the Switch when
    // the host deletes or closes the current app.
    active_session_ptr()->m_is_active = false;
    if (action == artemis::streaming::ConnectionTerminatedAction::MarkTerminated)
        active_session_ptr()->m_is_terminated = true;
}

void MoonlightSession::connection_log_message(const char* format, ...) {
    va_list arglist;
    va_start(arglist, format);
    int size = vsnprintf(NULL, 0, format, arglist);
    char buffer[size];
    vsnprintf(buffer, size, format, arglist);
    va_end(arglist);

    brls::Logger::info(fmt::runtime(std::string(buffer)));
}

void MoonlightSession::connection_rumble(unsigned short controller,
                                         unsigned short lowFreqMotor,
                                         unsigned short highFreqMotor) {
    MoonlightInputManager::instance().handleRumble(controller, lowFreqMotor,
                                                   highFreqMotor);
}

void MoonlightSession::connection_rumble_triggers(uint16_t controllerNumber,
                                                  uint16_t leftTriggerMotor,
                                                  uint16_t rightTriggerMotor) {
    // MoonlightInputManager::instance().handleRumbleTriggers(controllerNumber, leftTriggerMotor, rightTriggerMotor);
}

void MoonlightSession::connection_status_update(int connection_status) {
    if (active_session_ptr()) {
        active_session_ptr()->m_connection_status_is_poor =
            connection_status == CONN_STATUS_POOR;
    }
}

void MoonlightSession::connection_set_hdr_mode(bool use_hdr) {
    if (active_session_ptr()) {
        active_session_ptr()->m_use_hdr = use_hdr;
    }
}

// MARK: Video decoder callbacks

int MoonlightSession::video_decoder_setup(int video_format, int width,
                                          int height, int redraw_rate,
                                          void* context, int dr_flags) {
    m_video_format = video_format;
    if (active_session_ptr() && !active_session_ptr()->m_input_only &&
        active_session_ptr()->m_video_decoder) {
        return active_session_ptr()->m_video_decoder->setup(
            video_format, width, height, redraw_rate, context, dr_flags);
    }
    return DR_OK;
}

void MoonlightSession::video_decoder_start() {
    if (active_session_ptr() && active_session_ptr()->m_video_decoder) {
        active_session_ptr()->m_video_decoder->start();
    }
}

void MoonlightSession::video_decoder_stop() {
    if (active_session_ptr() && active_session_ptr()->m_video_decoder) {
        active_session_ptr()->m_video_decoder->stop();
    }
}

void MoonlightSession::video_decoder_cleanup() {
    if (active_session_ptr() && active_session_ptr()->m_video_decoder) {
        active_session_ptr()->m_video_decoder->cleanup();
    }
}

int MoonlightSession::video_decoder_submit_decode_unit(PDECODE_UNIT decode_unit) {
    if (active_session_ptr() && active_session_ptr()->m_video_decoder) {
        return active_session_ptr()->m_video_decoder->submit_decode_unit(decode_unit);
    }
    return DR_OK;
}

// MARK: Audio callbacks

int MoonlightSession::audio_renderer_init(
    int audio_configuration, const POPUS_MULTISTREAM_CONFIGURATION opus_config,
    void* context, int ar_flags) {
    if (active_session_ptr() && !active_session_ptr()->m_input_only &&
        active_session_ptr()->m_audio_renderer) {
        return active_session_ptr()->m_audio_renderer->init(
            audio_configuration, opus_config, context, ar_flags);
    }
    return DR_OK;
}

void MoonlightSession::audio_renderer_start() {
    if (active_session_ptr() && active_session_ptr()->m_audio_renderer) {
        active_session_ptr()->m_audio_renderer->start();
    }
}

void MoonlightSession::audio_renderer_stop() {
    if (active_session_ptr() && active_session_ptr()->m_audio_renderer) {
        active_session_ptr()->m_audio_renderer->stop();
    }
}

void MoonlightSession::audio_renderer_cleanup() {
    if (active_session_ptr() && active_session_ptr()->m_audio_renderer) {
        active_session_ptr()->m_audio_renderer->cleanup();
    }
}

void MoonlightSession::audio_renderer_decode_and_play_sample(
    char* sample_data, int sample_length) {
    if (active_session_ptr() && active_session_ptr()->m_audio_renderer) {
        active_session_ptr()->m_audio_renderer->decode_and_play_sample(
            sample_data, sample_length);
    }
}

// MARK: MoonlightSession

void MoonlightSession::start(ServerCallback<bool> callback, bool is_sunshine) {
    m_is_sunshine = is_sunshine;
    m_stop_requested = false;
    m_is_terminated = false;

    LiInitializeStreamConfiguration(&m_config);

    int w = 0;
    int h = 0;
    const auto custom = artemis::streaming::StreamProfileStore::instance().get();
    if (custom.customResolutionEnabled) {
        w = custom.width;
        h = custom.height;
        brls::Logger::info("Artemis custom stream resolution: {}x{}", w, h);
    }     else {
        const int resolution = Settings::instance().resolution();
        h = resolution;
        w = artemis::streaming::streamWidthFromHeight(
            h, Settings::instance().aspect_ratio());
        if (resolution == -1) {
#if defined(PLATFORM_IOS) || defined(PLATFORM_VISIONOS)
            getWindowSize(&w, &h);
#else
            h = Application::windowHeight;
            w = Application::windowWidth;
#endif

            const int nativeResolutionScale = Settings::instance().native_resolution_scale();
            if (nativeResolutionScale != 100 && w > 0 && h > 0) {
                w = (w * nativeResolutionScale + 50) / 100;
                h = (h * nativeResolutionScale + 50) / 100;
            }
        }
    }

    // Video dimensions must be even for the decoder and common YUV formats.
    if ((h & 1) == 1)
        h += 1;
    if ((w & 1) == 1)
        w += 1;

    m_config.width = w;
    m_config.height = h;
    m_config.fps = Settings::instance().fps();
    if (m_apollo_launch_options.virtualDisplay &&
        m_apollo_launch_options.width > 0 &&
        m_apollo_launch_options.height > 0) {
        m_config.width = m_apollo_launch_options.width;
        m_config.height = m_apollo_launch_options.height;
        if (m_apollo_launch_options.refreshRate > 0)
            m_config.fps = m_apollo_launch_options.refreshRate;
    }
    m_config.clientRefreshRateX100 =
        artemis::stream::resolveClientRefreshRateX100(
            m_config.fps, Settings::instance().client_refresh_rate_x100());
    m_config.audioConfiguration =
        Settings::instance().stream_audio_configuration() ==
                STREAM_AUDIO_51_SURROUND
            ? AUDIO_CONFIGURATION_51_SURROUND
            : AUDIO_CONFIGURATION_STEREO;
    {
        const auto advanced =
            artemis::stream::AdvancedStreamOptionsStore::instance().get();
        m_config.packetSize =
            artemis::stream::resolveStreamPacketSize(advanced);
    }
    m_config.streamingRemotely = STREAM_CFG_AUTO;
    m_config.bitrate = Settings::instance().bitrate();
    m_config.encryptionFlags = m_is_sunshine ? ENCFLG_ALL : ENCFLG_VIDEO;

    switch (Settings::instance().video_codec()) {
    case H264:
        m_config.supportedVideoFormats = artemis::stream::supportedVideoFormats(
            artemis::stream::StreamVideoCodec::H264,
            Settings::instance().request_hdr());
        break;
    case H265:
        m_config.supportedVideoFormats = artemis::stream::supportedVideoFormats(
            artemis::stream::StreamVideoCodec::H265,
            Settings::instance().request_hdr());
        break;
    case AV1:
        m_config.supportedVideoFormats = artemis::stream::supportedVideoFormats(
            artemis::stream::StreamVideoCodec::AV1,
            Settings::instance().request_hdr());
        break;
    default:
        break;
    }

    LiInitializeConnectionCallbacks(&m_connection_callbacks);
    m_connection_callbacks.stageStarting = connection_stage_starting;
    m_connection_callbacks.stageComplete = connection_stage_complete;
    m_connection_callbacks.stageFailed = connection_stage_failed;
    m_connection_callbacks.connectionStarted = connection_started;
    m_connection_callbacks.connectionTerminated = connection_terminated;
    m_connection_callbacks.logMessage = connection_log_message;
    m_connection_callbacks.rumble = connection_rumble;
    m_connection_callbacks.rumbleTriggers = connection_rumble_triggers;
    m_connection_callbacks.connectionStatusUpdate = connection_status_update;
    m_connection_callbacks.setHdrMode = connection_set_hdr_mode;

    LiInitializeVideoCallbacks(&m_video_callbacks);
    m_video_callbacks.setup = video_decoder_setup;
    m_video_callbacks.start = video_decoder_start;
    m_video_callbacks.stop = video_decoder_stop;
    m_video_callbacks.cleanup = video_decoder_cleanup;
    m_video_callbacks.submitDecodeUnit = video_decoder_submit_decode_unit;

    if (m_video_decoder)
        m_video_callbacks.capabilities = m_video_decoder->capabilities();

    LiInitializeAudioCallbacks(&m_audio_callbacks);
    m_audio_callbacks.init = audio_renderer_init;
    m_audio_callbacks.start = audio_renderer_start;
    m_audio_callbacks.stop = audio_renderer_stop;
    m_audio_callbacks.cleanup = audio_renderer_cleanup;
    m_audio_callbacks.decodeAndPlaySample = audio_renderer_decode_and_play_sample;

    if (m_audio_renderer)
        m_audio_callbacks.capabilities = m_audio_renderer->capabilities();

    GameStreamClient::instance().start(
        m_address, m_config, m_app_id, [this, callback](auto result) {
            if (result.isSuccess()) {
                m_config = result.value();
                brls::async([this, callback]() mutable {
                    auto m_data = GameStreamClient::instance().server_data(m_address);

                    // Advisory: if the host does not advertise the requested
                    // codec profile, warn without blocking the stream. The manual
                    // codec choice remains authoritative.
                    const auto codecs = artemis::host::decodeServerCodecMask(
                        m_data.serverInfo.serverCodecModeSupport);
                    const artemis::host::CodecKind chosen =
                        Settings::instance().video_codec() == H264
                            ? artemis::host::CodecKind::H264
                            : (Settings::instance().video_codec() == AV1
                                   ? artemis::host::CodecKind::AV1
                                   : artemis::host::CodecKind::H265);
                    if (!artemis::host::supportsCodec(
                            codecs, chosen, Settings::instance().request_hdr())) {
                        brls::Logger::warning(
                            "MoonlightSession: host does not advertise the "
                            "requested codec (mask 0x{:X}); falling back to the "
                            "host's negotiated format",
                            m_data.serverInfo.serverCodecModeSupport);
                    }

                    int result = LiStartConnection(
                        &m_data.serverInfo, &m_config, &m_connection_callbacks,
                        &m_video_callbacks, &m_audio_callbacks, NULL, 0, NULL, 0);

                    brls::sync([callback, result] {
                        if (result != 0) {
                            // Do not LiStopConnection here: this can still race
                            // the in-flight deko3d present. Mark dead and let
                            // StreamingView::terminate() stop on the next frame.
                            if (active_session_ptr()) {
                                active_session_ptr()->m_is_active = false;
                                active_session_ptr()->m_is_terminated = true;
                            }
                            if (Settings::instance().video_codec() == AV1) {
                                callback(GSResult<bool>::failure(
                                    "error/av1_decoder_unavailable"_i18n));
                            } else {
                                callback(GSResult<bool>::failure(
                                    "error/stream_start"_i18n));
                            }
                        } else {
                            callback(GSResult<bool>::success(true));
                        }
                    });
                });
            } else {
                brls::Logger::error(
                    "MoonlightSession: Failed to start stream: {}",
                    result.error().c_str());
                callback(GSResult<bool>::failure(result.error()));
            }
        }, m_apollo_launch_options);
}

void MoonlightSession::stop(int terminate_app, bool sync_host_quit) {
    if (m_stop_requested)
        return;

    m_stop_requested = true;
    m_restart_in_progress = false;

    // Stop dispatching to callbacks as early as possible: worker threads check
    // the active pointer, so clearing it here (not only in the destructor)
    // narrows the window in which they could keep dereferencing this session.
    m_active_session.store(nullptr, std::memory_order_release);

    // Sunshine can leave the host session paused after LiStopConnection alone
    // (community report: Moonlight-Switch #215). Always cancel for Sunshine so
    // other clients can reconnect without restarting the host service.
    if (terminate_app || m_is_sunshine) {
        if (sync_host_quit)
            GameStreamClient::instance().quitNow(m_address);
        else
            GameStreamClient::instance().quit(m_address, [](auto _) {});
    }

    LiStopConnection();
}

void MoonlightSession::stopForApplicationExit() {
    stop(/*terminate_app=*/1, /*sync_host_quit=*/true);
}

void MoonlightSession::restart() {
    // Mark the old stream inactive before stopping it so callers can wait for
    // the new connection instead of accidentally measuring the old session.
    m_is_active.store(false);
    m_is_terminated.store(false);
    m_restart_in_progress.store(true);
    LiStopConnection();

    start([](const GSResult<bool>& result) {
        if (result.isSuccess()) {
            brls::Logger::info("MoonlightSession: Reconnected");
        } else {
            brls::Logger::info("MoonlightSession: Reconnection failed");
            if (active_session_ptr()) {
                active_session_ptr()->m_restart_in_progress = false;
                active_session_ptr()->m_is_active = false;
                active_session_ptr()->m_is_terminated = true;
            }
        }
    }, active_session_ptr()->m_is_sunshine);
}

void MoonlightSession::restartWithApolloOptions(
    APOLLO_LAUNCH_OPTIONS options, ServerCallback<bool> callback) {
    m_is_active.store(false);
    m_is_terminated.store(false);
    m_restart_in_progress.store(true);
    LiStopConnection();
    m_apollo_launch_options = std::move(options);

    start([callback](const GSResult<bool>& result) {
        if (active_session_ptr())
            active_session_ptr()->m_restart_in_progress = false;
        callback(result);
    }, m_is_sunshine);
}

void MoonlightSession::draw(NVGcontext* vg, int width, int height) {
    if (!artemis::streaming::shouldPresentStreamFrame(m_is_terminated.load(),
                                                      m_is_active.load()))
        return;

    if (!m_input_only && m_video_decoder && m_video_renderer) {
        AVFrameHolder::instance().get(
            [this, vg, width, height](AVFrame* frame) {
                m_video_renderer->draw(vg, width, height, frame, m_video_format);
            });

        const auto pipe =
            artemis::streaming::FramePipelineTelemetry::instance().snapshot();
        AVFrameHolder::instance().updatePresentCostMs(
            pipe.renderMsP95, pipe.gpuSubmitMsP95, pipe.havePresentCostSamples);

        const uint64_t now = LiGetMillis();
        if (m_last_stats_update_ms == 0 || now - m_last_stats_update_ms >= 250) {
            m_session_stats.video_decode_stats = *m_video_decoder->video_decode_stats();
            m_session_stats.video_render_stats = *m_video_renderer->video_render_stats();
            m_last_stats_update_ms = now;
        }
    }
}
