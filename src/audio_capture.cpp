// ====================================================================
// AUDIO CAPTURE - Implementation
// ====================================================================
#include "audio_capture.h"
#include <alsa/asoundlib.h>
#include "../kissfft/kiss_fft.h"
#include <cmath>
#include <iostream>
#include <cstring>

AudioCapture::AudioCapture() {
    memset(&m_audioData, 0, sizeof(m_audioData));
}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start() {
    if (m_running) return true;

    m_running = true;
    m_thread = std::thread(&AudioCapture::captureThread, this);
    return true;
}

void AudioCapture::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

AudioData AudioCapture::getAudioData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_audioData;
}

void AudioCapture::setSensitivity(float sensitivity) {
    m_sensitivity.store(sensitivity);
}

void AudioCapture::captureThread() {
    snd_pcm_t* handle;
    int err;

    // Try different device names
    const char* devices[] = {
        "plughw:0,0",
        "plughw:1,0",
        "hw:0,0",
        "hw:1,0",
        "default",
        nullptr
    };

    for (int i = 0; devices[i] != nullptr; i++) {
        err = snd_pcm_open(&handle, devices[i], SND_PCM_STREAM_CAPTURE, 0);
        if (err >= 0) {
            std::cerr << "Opened audio device: " << devices[i] << std::endl;
            break;
        }
    }

    if (err < 0) {
        std::cerr << "ALSA error: " << snd_strerror(err) << std::endl;
        std::cerr << "Could not open any audio device" << std::endl;
        m_running = false;
        return;
    }

    // Set parameters
    err = snd_pcm_set_params(handle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        1,      // Mono
        44100,  // Sample rate
        1,      // Allow resampling
        500000  // Latency: 500ms
    );

    if (err < 0) {
        // Try 48kHz
        err = snd_pcm_set_params(handle,
            SND_PCM_FORMAT_S16_LE,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            1,
            48000,
            1,
            500000
        );
        if (err < 0) {
            std::cerr << "Set params error: " << snd_strerror(err) << std::endl;
            snd_pcm_close(handle);
            m_running = false;
            return;
        }
        std::cerr << "Using 48kHz sample rate" << std::endl;
    } else {
        std::cerr << "Using 44.1kHz sample rate" << std::endl;
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        std::cerr << "Prepare error: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        m_running = false;
        return;
    }

    err = snd_pcm_start(handle);
    if (err < 0) {
        std::cerr << "Start error: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        m_running = false;
        return;
    }

    std::cerr << "Audio capture started" << std::endl;

    const int N = 1024;
    int16_t buffer[N];
    float samples[N];

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, nullptr, nullptr);
    kiss_fft_cpx in[N], out[N];

    while (m_running) {
        int frames = snd_pcm_readi(handle, buffer, N);

        if (frames < 0) {
            if (frames == -EPIPE) {
                snd_pcm_prepare(handle);
                snd_pcm_start(handle);
            } else if (frames == -EIO) {
                snd_pcm_drop(handle);
                snd_pcm_prepare(handle);
                snd_pcm_start(handle);
            } else {
                snd_pcm_recover(handle, frames, 0);
            }
            continue;
        }

        float sensitivity = m_sensitivity.load();

        // Convert to float
        for (int i = 0; i < N; i++) {
            samples[i] = buffer[i] / 32768.0f;
        }

        // Calculate volume
        float vol = 0;
        for (int i = 0; i < N; i++) {
            vol += samples[i] * samples[i];
        }
        vol = sqrt(vol / N) * sensitivity;

        // FFT
        for (int i = 0; i < N; i++) {
            in[i].r = samples[i];
            in[i].i = 0;
        }
        kiss_fft(cfg, in, out);

        // 8-band spectrum
        float spectrum[8];
        int halfN = N / 2;
        int bandSize = halfN / 8;

        for (int b = 0; b < 8; b++) {
            float energy = 0;
            int start = bandSize * b;
            int end = start + bandSize;

            for (int i = start; i < end; i++) {
                energy += sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
            }
            spectrum[b] = (energy / bandSize) * sensitivity;
        }

        // Bass, mid, treble
        float bass = (spectrum[0] + spectrum[1]) / 2;
        float mid = (spectrum[2] + spectrum[3] + spectrum[4]) / 3;
        float treble = (spectrum[5] + spectrum[6] + spectrum[7]) / 3;

        // Beat detection
        float low = spectrum[0] + spectrum[1] + spectrum[2];
        float diff = low - m_lastEnergy;
        m_lastEnergy = m_lastEnergy * 0.95f + low * 0.05f;

        if (diff > 0.1f) {
            m_beatSmooth = 1.0f;
        } else {
            m_beatSmooth = m_beatSmooth * 0.92f;
        }

        // Update audio data
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_audioData.volume = vol;
            m_audioData.beat = m_beatSmooth;
            m_audioData.bass = bass;
            m_audioData.mid = mid;
            m_audioData.treble = treble;
            for (int i = 0; i < 8; i++) {
                m_audioData.spectrum[i] = spectrum[i];
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    free(cfg);
    snd_pcm_close(handle);
    std::cerr << "Audio capture stopped" << std::endl;
}
