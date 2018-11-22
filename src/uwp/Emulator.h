#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <NDS.h>

#include <concrt.h>
#include <ppltasks.h>

#include <SDL_audio.h>

#include <d3d11.h>
#include <SimpleMath.h>

#include "VirtualController.h"

using namespace concurrency;
using namespace DirectX::SimpleMath;

namespace uwp 
{

class Emulator
{
    critical_section m_emuLock;
    
    bool m_romLoaded = false;
    bool m_emulating = false;

    task<void> m_emuLoop;
    
    std::atomic<bool> m_framebufferUploaded;

    std::mutex m_framebufferUploadMutex;
    std::condition_variable m_framebufferUploadCV;

    clock_t m_deltaTime;
    std::uint64_t m_frames;

    double m_fps;
    SDL_AudioDeviceID m_device;

    std::mutex m_scaleMutex;

    Vector2 m_scale{ 1.0f };

    Vector2 m_screen1Pos{ .0f };
    Vector2 m_screen2Pos{ .0f };

    bool m_touching = false;
    Vector2 m_touchPos { .0f };

    std::unique_ptr<VirtualController>   m_controller;

public:
    VirtualController *GetVirtualController()
    {
        return m_controller.get();
    }

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

    bool IsEmulating() const
    {
        return m_emulating;
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

    void SetScale(const Vector2 &v)
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        m_scale = v;
    }

    Vector2 GetScale()
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        return m_scale;
    }

    void SetScreen1Pos(const Vector2 &v)
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        m_screen1Pos = v;
    }  
    
    void SetScreen2Pos(const Vector2 &v)
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        m_screen2Pos = v;
    }

    Vector2 GetScreen1Pos()
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        return m_screen1Pos;
    }

    Vector2 GetScreen2Pos()
    {
        const std::lock_guard<std::mutex> guard(m_scaleMutex);
        return m_screen2Pos;
    }

    bool IsTouching() const
    {
        return m_touching;
    }

    void SetKeyPressed(const u32 key);
    void SetKeyReleased(const u32 key);

    void Touch(const Vector2 &pos);
    void ReleaseScreen();

    void WaitForLoop()
    {
        m_emuLoop.wait();
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