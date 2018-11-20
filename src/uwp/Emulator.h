#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <NDS.h>

#include <concrt.h>
#include <ppltasks.h>

#include <xaudio2.h>

using namespace concurrency;

namespace uwp 
{

class Emulator
{
    friend class VoiceCallback;

    critical_section m_emuLock;
    
    bool m_romLoaded = false;
    bool m_emulating = false;

    task<void> m_emuLoop;
    task<void> m_emuVoiceLoop;

    std::atomic<bool> m_framebufferUploaded;

    std::mutex m_framebufferUploadMutex;
    std::condition_variable m_framebufferUploadCV;

    clock_t m_deltaTime;
    std::uint64_t m_frames;

    double m_fps;

    Microsoft::WRL::ComPtr<IXAudio2> m_audioDev; 
    IXAudio2MasteringVoice* m_masterVoice;
    IXAudio2SourceVoice* m_sourceVoice;

    std::array<u8, 710 * 8> m_audioData;
    //std::shared_ptr

public:
    task<bool> LoadROM(Platform::String ^path, Platform::String ^sramFile, bool direct);
        
    void Init();
    void Start();

    void Shutdown();

    double GetFps() const
    {
        return m_fps;
    }

    void SetEmulate(bool val)
    {
        m_emulating = val;
    }

    void NotifyUploadDone()
    {
        m_framebufferUploaded = true;
        m_framebufferUploadCV.notify_all();
    }

    bool IsUploadPending()
    {
        return m_romLoaded && m_framebufferUploaded == false;
    }
};

[Windows::UI::Xaml::Data::Bindable]
public ref class EmulatorWrapper sealed
{
internal:
    std::shared_ptr<uwp::Emulator> m_emu;
    EmulatorWrapper(const std::shared_ptr<Emulator> &emu) : m_emu(emu) {}

public:
    property Platform::String^ Name
    {
        Platform::String^ get()
        {
            return "MelonDS";
        }
    }
};

}