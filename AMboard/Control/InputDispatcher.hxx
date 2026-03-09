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

private:
    CInputDispatcher() = default;
    ~CInputDispatcher() { Stop(); }

    std::atomic<bool> m_playing { false };
    std::thread m_playbackThread;
    std::condition_variable m_cv;
    std::mutex m_cvMutex;

    void PlaybackLoop(std::vector<SPlaybackEvent> sequence);
    void InjectEvent(const SInputEvent& ev);
};