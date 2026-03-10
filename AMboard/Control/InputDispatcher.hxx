//
// Created by LYS on 3/9/2026.
//

#pragma once

#include "InputService.hxx"

struct SPlaybackEvent {
    uint32_t delayMs;
    SInputEvent event;
};

class CInputDispatcher {
public:
    static CInputDispatcher& Get();

    // Starts playing a sequence of events in a background thread
    void Play(const std::vector<SPlaybackEvent>& sequence);

    // Instantly stops playback, interrupting any ongoing delays
    void Stop();

    bool IsPlaying() const;

    [[nodiscard]] auto GetPlaybackCount() const noexcept { return m_CurrentPlaybackCount;}
    [[nodiscard]] auto GetPlaybackProgress() const noexcept { return m_CurrentPlaybackProgress;}

private:
    CInputDispatcher() = default;
    ~CInputDispatcher() { Stop(); }

    std::atomic<bool> m_playing { false };
    std::thread m_playbackThread;
    std::condition_variable m_cv;
    std::mutex m_cvMutex;

    volatile size_t m_CurrentPlaybackCount { 0 };
    volatile size_t m_CurrentPlaybackProgress { 0 };

    void PlaybackLoop(std::vector<SPlaybackEvent> Sequence);
    void InjectEvent(const SInputEvent& ev);
};