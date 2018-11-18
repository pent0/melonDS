#include "MainPage.xaml.h"

namespace uwp {
    MainPage::MainPage()
    {
        InitializeComponent();
    }

    void MainPage::OnHamburgerButtonClick(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e)
    {
        // Toggle pane open
        MainSplitView->IsPaneOpen = !MainSplitView->IsPaneOpen;
    }
}