#pragma once

#include "MainPage.g.h"

namespace uwp
{
    /// <summary>
    /// The main page
    /// </summary>
    public ref class MainPage sealed
    {
    protected:
        void OnHamburgerButtonClick(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

    public:
        MainPage();
    };
}

