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

        m_dpadRect.x = settings->PadLeftL * vscale / 2.54f * width;
        m_dpadRect.y = settings->PadBottomL * vscale / 2.54f * height;
        m_aRect.x = settings->ACenterXP * vscale / 2.54f * width;
        m_aRect.y = settings->ACenterYL * vscale / 2.54f * height;
        m_bRect.x = settings->BCenterXP * vscale / 2.54f * width;
        m_bRect.y = settings->BCenterYL * vscale / 2.54f * height;
        m_startRect.x = settings->StartXP * vscale / 2.54f * width;
        m_startRect.y = settings->StartYL * vscale / 2.54f * height;
        m_selectRect.x = settings->SelectXP * vscale / 2.54f * width;
        m_selectRect.y = settings->SelectYL * vscale / 2.54f * height;
        m_lRect.x = settings->LeftXP * vscale / 2.54f * width;
        m_lRect.y = settings->LeftYL * vscale / 2.54f * height;
        m_rRect.x = settings->RightXP * vscale / 2.54f * width;
        m_rRect.y = settings->RightYL * vscale / 2.54f * height;
        m_xRect.x = settings->XCenterXP * vscale / 2.54f * width;
        m_xRect.y = settings->XCenterYL * vscale / 2.54f * height;
        m_yRect.x = settings->YCenterXP * vscale / 2.54f * width;
        m_yRect.y = settings->YCenterYL * vscale / 2.54f * height;
    } 
    else
    {
        auto hscale = pow(width * 2.54f / 8.0f, 0.75f);
        auto vscale = pow(height * 2.54f / 8.0f, 0.75f);

        m_dpadRect.x = settings->PadLeftP * vscale / 2.54f * width;
        m_dpadRect.y = settings->PadBottomP * vscale / 2.54f * height;
        m_aRect.x = settings->ACenterXP * vscale / 2.54f * width;
        m_aRect.y = settings->ACenterYP * vscale / 2.54f * height;
        m_bRect.x = settings->BCenterXP * vscale / 2.54f * width;
        m_bRect.y = settings->BCenterYP * vscale / 2.54f * height;
        m_startRect.x = settings->StartXP * vscale / 2.54f * width;
        m_startRect.y = settings->StartYP * vscale / 2.54f * height;
        m_selectRect.x = settings->SelectXP * vscale / 2.54f * width;
        m_selectRect.y = settings->SelectYP * vscale / 2.54f * height;
        m_lRect.x = settings->LeftXP * vscale / 2.54f * width;
        m_lRect.y = settings->LeftYP * vscale / 2.54f * height;
        m_rRect.x = settings->RightXP * vscale / 2.54f * width;
        m_rRect.y = settings->RightYP * vscale / 2.54f * height;
        m_xRect.x = settings->XCenterXP * vscale / 2.54f * width;
        m_xRect.y = settings->XCenterYP * vscale / 2.54f * height;
        m_yRect.x = settings->YCenterXP * vscale / 2.54f * width;
        m_yRect.y = settings->YCenterYP * vscale / 2.54f * height;
    }
}

void VirtualController::CalculateRenderRect()
{
    auto bounds = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBounds;
    auto rawScaleFactor = Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;

    float width = bounds.Width * rawScaleFactor;
    float height = bounds.Height * rawScaleFactor;


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
    for (const auto &infoP : m_pInfos)
    {
        Vector2 p = { infoP.second.m_point->Position.X, infoP.second.m_point->Position.Y };

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

        /*
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
        }*/
    }
}

}