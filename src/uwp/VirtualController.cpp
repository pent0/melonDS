#include "VirtualController.h"

using namespace Windows::Graphics::Display;

namespace uwp
{

ControllerState VirtualController::GetState()
{
    concurrency::critical_section::scoped_lock scopeLock(m_vcLock);
    return m_state;
}

void VirtualController::InitFromSettings(uwp::EmulatorConfig ^settings)
{
    auto displayInfo = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();

    auto bounds = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBounds;
    auto rawScaleFactor = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;

    float width = bounds.Width * rawScaleFactor;
    float height = bounds.Height * rawScaleFactor;

    if ((displayInfo->CurrentOrientation == DisplayOrientations::Landscape) || 
        (displayInfo->CurrentOrientation == DisplayOrientations::LandscapeFlipped))
    {
        auto hscale = pow(width * 2.54f / 8.0f, 0.75f);
        auto vscale = pow(height * 2.54f / 8.0f, 0.75f);

        m_dpadPos.x = settings->PadLeftL * vscale / 2.54f * width;
        m_dpadPos.y = height - settings->PadBottomL * vscale / 2.54f * height;

        m_aPos.x = settings->ACenterXP * vscale / 2.54f * width;
        m_aPos.y = settings->ACenterYL * vscale / 2.54f * height;
        m_bPos.x = settings->BCenterXP * vscale / 2.54f * width;
        m_bPos.y = settings->BCenterYL * vscale / 2.54f * height;
        m_startPos.x = settings->StartXP * vscale / 2.54f * width;
        m_startPos.y = settings->StartYL * vscale / 2.54f * height;
        m_selectPos.x = settings->SelectXP * vscale / 2.54f * width;
        m_selectPos.y = settings->SelectYL * vscale / 2.54f * height;
        m_lPos.x = settings->LeftXP * vscale / 2.54f * width;
        m_lPos.y = settings->LeftYL * vscale / 2.54f * height;
        m_rPos.x = settings->RightXP * vscale / 2.54f * width;
        m_rPos.y = settings->RightYL * vscale / 2.54f * height;
        m_xPos.x = settings->XCenterXP * vscale / 2.54f * width;
        m_xPos.y = settings->XCenterYL * vscale / 2.54f * height;
        m_yPos.x = settings->YCenterXP * vscale / 2.54f * width;
        m_yPos.y = settings->YCenterYL * vscale / 2.54f * height;
    } 
    else
    {
        auto hscale = pow(width * 2.54f / 8.0f, 0.75f);
        auto vscale = pow(height * 2.54f / 8.0f, 0.75f);

        m_dpadPos.x = settings->PadLeftP * vscale / 2.54f * width;
        m_dpadPos.y = settings->PadBottomP * vscale / 2.54f * height;
        m_aPos.x = settings->ACenterXP * vscale / 2.54f * width;
        m_aPos.y = settings->ACenterYP * vscale / 2.54f * height;
        m_bPos.x = settings->BCenterXP * vscale / 2.54f * width;
        m_bPos.y = settings->BCenterYP * vscale / 2.54f * height;
        m_startPos.x = settings->StartXP * vscale / 2.54f * width;
        m_startPos.y = settings->StartYP * vscale / 2.54f * height;
        m_selectPos.x = settings->SelectXP * vscale / 2.54f * width;
        m_selectPos.y = settings->SelectYP * vscale / 2.54f * height;
        m_lPos.x = settings->LeftXP * vscale / 2.54f * width;
        m_lPos.y = settings->LeftYP * vscale / 2.54f * height;
        m_rPos.x = settings->RightXP * vscale / 2.54f * width;
        m_rPos.y = settings->RightYP * vscale / 2.54f * height;
        m_xPos.x = settings->XCenterXP * vscale / 2.54f * width;
        m_xPos.y = settings->XCenterYP * vscale / 2.54f * height;
        m_yPos.x = settings->YCenterXP * vscale / 2.54f * width;
        m_yPos.y = settings->YCenterYP * vscale / 2.54f * height;
    }
}

void VirtualController::CalculateRenderRect()
{
    auto bounds = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBounds;
    auto rawScaleFactor = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;

    float width = bounds.Width * rawScaleFactor;
    float height = bounds.Height * rawScaleFactor;

    auto hscale = pow(width * 2.54f / 8.0f, 0.75f);
    auto vscale = pow(height * 2.54f / 8.0f, 0.75f);

    m_dpadRect.x = m_dpadPos.x;
    m_dpadRect.y = height - m_dpadPos.y - 120 * vscale * m_controllerScale;
    m_dpadRect.width = 120 * vscale * m_controllerScale;
    m_dpadRect.height = 120 * vscale * m_controllerScale;

    m_aRect.x = width - m_aPos.x - 30 * vscale * m_controllerScale;
    m_aRect.y = height - m_aPos.y - 30 * vscale * m_controllerScale;
    m_aRect.width = 60 * vscale * m_controllerScale;
    m_aRect.height = 60 * vscale * m_controllerScale;

    m_bRect.x = width - m_bPos.x - 30 * vscale * m_controllerScale;
    m_bRect.y = height - m_bPos.y - 30 * vscale * m_controllerScale;
    m_bRect.width = 60 * vscale * m_controllerScale;
    m_bRect.height = 60 * vscale * m_controllerScale;

    m_xRect.x = width - m_xPos.x - 30 * vscale * m_controllerScale;
    m_xRect.y = height - m_xPos.y - 30 * vscale * m_controllerScale;
    m_xRect.width = 60 * vscale * m_controllerScale;
    m_xRect.height = 60 * vscale * m_controllerScale;

    m_yRect.x = width - m_yPos.x - 30 * vscale * m_controllerScale;
    m_yRect.y = height - m_yPos.y - 30 * vscale * m_controllerScale;
    m_yRect.width = 60 * vscale * m_controllerScale;
    m_yRect.height = 60 * vscale * m_controllerScale;

    m_startRect.x = width / 2.0f + m_startPos.x - 25 * vscale *m_controllerScale;
    m_startRect.y = height / 2.0f - m_startPos.y - 25 * vscale * m_controllerScale;
    m_startRect.width = 50 * vscale * m_controllerScale;
    m_startRect.height = 25 * vscale * m_controllerScale;

    m_selectRect.x = width / 2.0f + m_selectPos.x - 25 * vscale *m_controllerScale;
    m_selectRect.y = height / 2.0f - m_selectPos.y - 25 * vscale * m_controllerScale;
    m_selectRect.width = 50 * vscale * m_controllerScale;
    m_selectRect.height = 25 * vscale * m_controllerScale;

    m_lRect.x = m_lPos.x - 45 * vscale * m_controllerScale;
    m_lRect.y = height - m_lPos.y - 13 * vscale *  m_controllerScale;
    m_lRect.width = 45 * vscale * m_controllerScale;
    m_lRect.height = 26 * vscale * m_controllerScale;

    m_rRect.x = m_rPos.x - 45 * vscale * m_controllerScale;
    m_rRect.y = height - m_rPos.y - 13 * vscale *  m_controllerScale;
    m_rRect.width = 45 * vscale * m_controllerScale;
    m_rRect.height = 26 * vscale * m_controllerScale;

    m_leftRect.x = m_dpadRect.x;
    m_leftRect.y = m_dpadRect.y;
    m_leftRect.width = m_dpadRect.width / 3.0f;
    m_leftRect.height = m_dpadRect.height;

    m_rightRect.width = m_dpadRect.width / 3.0f;
    m_rightRect.x = m_dpadRect.x + 2.0f * m_rightRect.width;
    m_rightRect.height = m_dpadRect.height;
    m_rightRect.y = m_dpadRect.y;

    m_upRect.x = m_dpadRect.x;
    m_upRect.y = m_dpadRect.y;
    m_upRect.width = m_dpadRect.width;
    m_upRect.height = m_dpadRect.height / 3.0f;

    m_downRect.height = m_dpadRect.height / 3.0f;
    m_downRect.y = m_dpadRect.y + 2.0f * m_downRect.height;
    m_downRect.x = m_dpadRect.x;
    m_downRect.width = m_dpadRect.width;
}

void VirtualController::OnPointerPressed(Windows::UI::Input::PointerPoint ^point)
{
    concurrency::critical_section::scoped_lock scopeLock(m_vcLock);

    PointerInfo info;
    info.m_point = point;
    info.m_isMoving = false;

    m_pInfos.emplace(point->PointerId, info);
}

void VirtualController::OnPointerMoved(Windows::UI::Input::PointerPoint ^point)
{
    concurrency::critical_section::scoped_lock scopeLock(m_vcLock);

    if (m_pInfos.find(point->PointerId) != m_pInfos.end())
    {
        m_pInfos[point->PointerId].m_isMoving = true;
        m_pInfos[point->PointerId].m_point = point;
    }
}

void VirtualController::OnPointerReleased(Windows::UI::Input::PointerPoint ^point)
{
    concurrency::critical_section::scoped_lock scopeLock(m_vcLock);

    if (m_pInfos.find(point->PointerId) != m_pInfos.end())
    {
        m_pInfos.erase(point->PointerId);
    }
}

void VirtualController::ResetState()
{
    ZeroMemory(&m_state, sizeof(ControllerState));
}

void VirtualController::Update()
{
    concurrency::critical_section::scoped_lock scopeLock(m_vcLock);
    ResetState();
    
    for (const auto &infoP : m_pInfos)
    {
        auto rawScaleFactor = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;
        Vector2 p = { 
            static_cast<float>(infoP.second.m_point->Position.X * rawScaleFactor),
            static_cast<float>(infoP.second.m_point->Position.Y * rawScaleFactor) };

        if (m_aRect.Contains(p))
        {
            m_state.m_a = true;
        }

        if (m_bRect.Contains(p))
        {
            m_state.m_b = true;
        }

        if (m_lRect.Contains(p))
        {
            m_state.m_l = true;
        }

        if (m_rRect.Contains(p))
        {
            m_state.m_r = true;
        }

        if (m_startRect.Contains(p))
        {
            m_state.m_start = true;
        }

        if (m_selectRect.Contains(p))
        {
            m_state.m_select = true;
        }

        if (m_upRect.Contains(p))
        {
            m_state.m_dpad[0] = true;
        }

        if (m_downRect.Contains(p))
        {
            m_state.m_dpad[1] = true;
        }

        if (m_leftRect.Contains(p))
        {
            m_state.m_dpad[2] = true;
        }

        if (m_rightRect.Contains(p))
        {
            m_state.m_dpad[3] = true;
        }
    }
}

}