//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
#include "DirectXPage.xaml.h"
#include "MainPage.xaml.h"

#include "Emulator.h"

#include <memory>

using namespace Windows::UI::Core;

namespace uwp
{
    /// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	public:
		App();
        virtual ~App();

		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;

	private:
        
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);
		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);

        void OnMainPageDisabled(Platform::Object ^sender);
        void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation ^sender,
            Platform::Object ^e);

        void OnWindowSizeChange(Platform::Object ^sender, WindowSizeChangedEventArgs ^e);
        void RecalculateScale();

		DirectXPage^ m_directXPage;
        MainPage ^m_mainPage;

        std::shared_ptr<uwp::Emulator> m_emulator;
        concurrency::task<bool> m_loadTask;
	};
}
