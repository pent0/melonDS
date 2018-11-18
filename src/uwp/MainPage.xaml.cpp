#include "MainPage.xaml.h"

#include <cstdio>

using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Interop;

namespace uwp {
    MainPage::MainPage()
    {
        InitializeComponent();
    }

    void MainPage::OnHamburgerButtonClick(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e)
    {
        // Toggle pane open
        MainSplitView->IsPaneOpen = false;
        Disabled(this);
    }

    void MainPage::OnNavigatedTo(NavigationEventArgs ^e) {
        Disabled += dynamic_cast<MainPageDisableEventHandler^>(e->Parameter);
    }
}