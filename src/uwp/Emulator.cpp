#include "Emulator.h"
#include "Common/Utility.h"

#include <GPU.h>
#include <SPU.h>

#include <cvt/wstring>
#include <codecvt>
#include <chrono>

#include <SDL.h>

using namespace std::chrono_literals;

namespace uwp
{

void EmulatorAudioCallback(void *userData, u8 *stream, int len)
{
    // resampling:
    // buffer length is 1024 samples
    // which is 710 samples at the original sample rate

    s16 buf_in[710 * 2];
    s16* buf_out = (s16*)stream;

    int num_in = SPU::ReadOutput(buf_in, 710);
    int num_out = 1024;

    int margin = 6;
    if (num_in < 710 - margin)
    {
        int last = num_in - 1;
        if (last < 0) last = 0;

        for (int i = num_in; i < 710 - margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = 710 - margin;
    }

    float res_incr = num_in / (float)num_out;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < 1024; i++)
    {
        // TODO: interp!!
        buf_out[i * 2] = buf_in[res_pos * 2];
        buf_out[i * 2 + 1] = buf_in[res_pos * 2 + 1];

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}

task<bool> Emulator::LoadROM(Platform::String ^path, Platform::String ^sramFile, bool direct)
{
    return create_task([this, path, sramFile, direct]() -> bool
    {
        critical_section::scoped_lock guard(m_emuLock);

        stdext::cvt::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        std::string pathUtf8 = convert.to_bytes(path->Data());
        std::string sramFileUtf8 = convert.to_bytes(sramFile->Data());

        m_romLoaded = NDS::LoadROM(pathUtf8.c_str(), sramFileUtf8.c_str(), direct);
        return m_romLoaded;
    });
}

void Emulator::Init()
{
    NDS::Init();

    SDL_AudioSpec spec;
    SDL_AudioSpec whatIwant, whatIget;

    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 47340;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = EmulatorAudioCallback;

    m_device = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, 0);

    if (!m_device)
    {
        auto a = SDL_GetError();
        int b = 5;
    }
}

void Emulator::Start()
{
    m_emuLoop = create_task([&]
    {
        if (!m_romLoaded || !m_emulating)
        {
            return;
        }

        if (m_device)
        {
            SDL_PauseAudioDevice(m_device, 0);
        }

        while (m_romLoaded && m_emulating)
        {
            critical_section::scoped_lock guard(m_emuLock);

            if (m_touching)
            {
                NDS::PressKey(16 + 6);
                NDS::TouchScreen(static_cast<u16>(m_touchPos.x), static_cast<u16>(m_touchPos.y));
            }

            clock_t beginc = clock();
            NDS::RunFrame();
            clock_t endc = clock();

            m_deltaTime += endc - beginc;
            m_frames++;

            if (ClockToMilliseconds(m_deltaTime) > 1000.0) {
                m_fps = (double)m_frames * 0.5 + m_fps * 0.5;
                m_frames = 0;
                m_deltaTime = 0;
            }

            // Wait for a graphics driver to upload the framebuffer
            m_framebufferUploaded = false;
            std::unique_lock<std::mutex> ulock(m_framebufferUploadMutex);

            // If after 10000ms, the framebuffer doesn't upload, the frame is runned as normal
            m_framebufferUploadCV.wait_for(ulock, 10000ms,
                [&]()
            {
                return m_framebufferUploaded == true;
            });
        }

        // Pause device again
        if (m_device)
        {
            SDL_PauseAudioDevice(m_device, 1);
        }
    });
}

void Emulator::Shutdown()
{
    m_emulating = false;
    m_emuLoop.wait();

    NDS::DeInit();

    if (m_device)
    {
        SDL_CloseAudioDevice(m_device);
    }
}

void Emulator::SetKeyPressed(const u32 key)
{
    critical_section::scoped_lock scopeLock(m_emuLock);
    NDS::PressKey(key);
}

void Emulator::SetKeyReleased(const u32 key)
{
    critical_section::scoped_lock scopeLock(m_emuLock);
    NDS::ReleaseKey(key);
}

void Emulator::Touch(const Vector2 &pos)
{
    critical_section::scoped_lock scopeLock(m_emuLock);

    m_touching = true;
    m_touchPos = pos;
}

void Emulator::ReleaseScreen()
{
    critical_section::scoped_lock scopeLock(m_emuLock);

    m_touching = false;
    NDS::ReleaseKey(16 + 6);

    NDS::ReleaseScreen();
}
}