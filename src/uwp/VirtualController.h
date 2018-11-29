#pragma once

#include <d3d11.h>

#include <concrt.h>
#include <SimpleMath.h>
#include <unordered_map>

#include "EmulatorConfig.h"

using namespace DirectX::SimpleMath;

namespace uwp
{

struct ControllerState
{
    // DPAD - Up0 Down1 Left2 Right3
    bool m_dpad[4];
    bool m_l;
    bool m_r;

    bool m_a;
    bool m_b;
    bool m_x;
    bool m_y;

    bool m_start;
    bool m_select;
};

struct PointerInfo
{
    Windows::UI::Input::PointerPoint ^m_point;
    bool m_isMoving = false;
};

class VirtualController
{
public:
    void OnPointerPressed(Windows::UI::Input::PointerPoint ^point);
    void OnPointerMoved(Windows::UI::Input::PointerPoint ^point);
    void OnPointerReleased(Windows::UI::Input::PointerPoint ^point);

    void Update();

    ControllerState GetState();

    explicit VirtualController(EmulatorConfig ^config)
    {
        InitFromSettings(config);
    }

private:    
    void ResetState();
    void CalculateRenderRect();

    void InitFromSettings(uwp::EmulatorConfig ^settings);

    Concurrency::critical_section m_vcLock;

    Rectangle m_lRect, m_rRect, m_aRect, m_bRect, m_xRect, m_yRect, m_startRect, m_selectRect,
        m_dpadRect, m_leftRect, m_rightRect, m_upRect, m_downRect;

    Vector2 m_lPos, m_rPos, m_aPos, m_bPos, m_xPos, m_yPos, m_startPos, m_selectPos, m_dpadPos;

    ControllerState m_state;

    double m_controllerScale;
    std::unordered_map<std::uint32_t, PointerInfo> m_pInfos;
};

}