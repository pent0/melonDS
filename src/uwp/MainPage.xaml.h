#pragma once

#include "MainPage.g.h"

namespace uwp
{
    public delegate void MainPageDisableEventHandler(Platform::Object ^sender);

    /// <summary>
    /// The main page
    /// </summary>
    public ref class MainPage sealed
    {
    protected:
        void OnHamburgerButtonClick(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs ^e) override;

    public:
        event MainPageDisableEventHandler ^Disabled;

        MainPage();
    };
}

