#include "Emulator.h"
#include "Common/Utility.h"

#include <GPU.h>
#include <SPU.h>

#include <cvt/wstring>
#include <codecvt>
#include <chrono>

using namespace std::chrono_literals;

namespace uwp
{

class VoiceCallback : public IXAudio2VoiceCallback
{
    std::shared_ptr<uwp::Emulator> m_emu;
public:
    HANDLE hBufferEndEvent;
    
    VoiceCallback(const std::shared_ptr<uwp::Emulator> emu) : 
        hBufferEndEvent(CreateEvent(NULL, FALSE, FALSE, NULL)),
        m_emu(emu) 
    {
    }

    ~VoiceCallback() { CloseHandle(hBufferEndEvent); }

    //Called when the voice has just finished playing a contiguous audio stream.
    void OnStreamEnd() { SetEvent(hBufferEndEvent); }

    //Unused methods are stubs
    void OnVoiceProcessingPassEnd() { }
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired)
    {
        s16 buf_in[710 * 2];
        s16* buf_out = (s16*)&m_emu->m_audioData[0];

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

    void OnBufferEnd(void * pBufferContext) { }
    void OnBufferStart(void * pBufferContext) {    }
    void OnLoopEnd(void * pBufferContext) {    }
    void OnVoiceError(void * pBufferContext, HRESULT Error) { }
};

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

    HRESULT hr = S_OK;

    // Get an interface to the main XAudio2 device
    hr = XAudio2Create(m_audioDev.GetAddressOf());

    // TODO:
    if (FAILED(hr));
    
    // Create master voice
    hr = m_audioDev->CreateMasteringVoice(&m_masterVoice);

    // TODO
    if (FAILED(hr));

    WAVEFORMATEX waveInfo;
    waveInfo.nChannels = 2;
    waveInfo.nSamplesPerSec = 48000;
    waveInfo.wBitsPerSample = 16;
    waveInfo.nAvgBytesPerSec = waveInfo.nSamplesPerSec * 4;
    waveInfo.nBlockAlign = 4;
    waveInfo.wFormatTag = WAVE_FORMAT_PCM;
    waveInfo.cbSize = 0;

    hr = m_audioDev->CreateSourceVoice(&m_sourceVoice, &waveInfo,  0,
        1.0f);

    if (FAILED(hr))
    {
        // TODO
    }
}

void Emulator::Start()
{
    m_emuLoop = create_task([&]
    {
        while (m_romLoaded && m_emulating)
        {
            critical_section::scoped_lock guard(m_emuLock);

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
    });

    m_emuVoiceLoop = create_task([&]
    {
        if (!m_sourceVoice)
        {
            return;
        }

        m_sourceVoice->Start();
    });
}

void Emulator::Shutdown()
{
    m_emulating = false;
    m_emuLoop.wait();

    NDS::DeInit();

    m_sourceVoice->DestroyVoice();
    m_masterVoice->DestroyVoice();
    m_audioDev.Reset();
}

}