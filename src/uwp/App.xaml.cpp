//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "DirectXPage.xaml.h"

#include <ppltasks.h>

#include <NDS.h>
#include <Config.h>

#include <SDL.h>

using namespace uwp;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Devices::Sensors;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {

    }

	InitializeComponent();
	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
}

App::~App()
{
    // Deinitialize all NDS resource
    if (m_emulator)
    {
        m_emulator->Shutdown();
    }

    SDL_Quit();
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used when the application is launched to open a specific file, to display
/// search results, and so forth.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
    m_emulator = std::make_shared<uwp::Emulator>();

    // Initialize NDS instance
    m_emulator->Init();

#if _DEBUG
	if (IsDebuggerPresent())
	{
		DebugSettings->EnableFrameRateCounter = true;
	}
#endif

	if (m_directXPage == nullptr)
	{
		m_directXPage = ref new DirectXPage();
	}

    // Create the main page
    if (m_mainPage == nullptr)
    {
        m_mainPage = ref new MainPage();
    }

	if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
	{
		m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
	}

	auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

	// Do not repeat app initialization when the Window already has content,
	// just ensure that the window is active
	if (rootFrame == nullptr)
	{
		// Create a Frame to act as the navigation context and associate it with
		// a SuspensionManager key
		rootFrame = ref new Frame();
		rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

        // Always set MainPage as the default page
		rootFrame->Navigate(TypeName(MainPage::typeid), ref new MainPageDisableEventHandler(this,
            &App::OnMainPageDisabled));

		// Place the frame in the current Window
		Window::Current->Content = rootFrame;
		// Ensure the current window is active
		Window::Current->Activate();
	}
	else
	{
		if (rootFrame->Content == nullptr)
		{
			// When the navigation stack isn't restored navigate to the first page,
			// configuring the new page by passing required information as a navigation
			// parameter
			rootFrame->Navigate(TypeName(MainPage::typeid), ref new MainPageDisableEventHandler(this,
                &App::OnMainPageDisabled));
		}
		// Ensure the current window is active
		Window::Current->Activate();
	}

    // Launch in fullscreen
    if (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Mobile") {
        ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();
    }

    Windows::Graphics::Display::DisplayInformation ^displayInfo =
        Windows::Graphics::Display::DisplayInformation::GetForCurrentView();

    displayInfo->OrientationChanged += ref new TypedEventHandler<DisplayInformation^, Object^>(
        this, &App::OnOrientationChanged);

    RecalculateScale();

    Window::Current->SizeChanged += ref new WindowSizeChangedEventHandler(this,
        &App::OnWindowSizeChange);

    Config::Threaded3D = 1;

    m_loadTask = m_emulator->LoadROM("wright.nds", "temp.sram", true);
}

/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
	(void) sender;	// Unused parameter
	(void) e;	// Unused parameter

	m_directXPage->SaveInternalState(ApplicationData::Current->LocalSettings->Values);
}

/// <summary>
/// Invoked when application execution is being resumed.
/// </summary>
/// <param name="sender">The source of the resume request.</param>
/// <param name="args">Details about the resume request.</param>
void App::OnResuming(Object ^sender, Object ^args)
{
	(void) sender; // Unused parameter
	(void) args; // Unused parameter

	m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e)
{
	throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

/// <summary>
/// Invoked when the main page is disabled
///
void App::OnMainPageDisabled(Platform::Object ^sender) 
{
    if (!m_loadTask.is_done())
    {
        m_loadTask.then([&](task<bool> res)
        {
            if (res.get())
            {
                m_emulator->SetEmulate(true);
                m_emulator->Start();
            }
        });
    }
    else
    {
        m_emulator->SetEmulate(true);
        m_emulator->Start();
    }

    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);
    rootFrame->Navigate(TypeName(DirectXPage::typeid), ref new EmulatorWrapper(m_emulator));
}

void App::OnOrientationChanged(Windows::Graphics::Display::DisplayInformation ^sender,
    Platform::Object ^e)
{
    switch (sender->CurrentOrientation)
    {
    case DisplayOrientations::Landscape:
    case DisplayOrientations::LandscapeFlipped:
    {
        Config::ScreenLayout = 2;
        RecalculateScale();

        break;
    }

    case DisplayOrientations::Portrait:
    case DisplayOrientations::PortraitFlipped:
    {
        Config::ScreenLayout = 0;
        RecalculateScale();

        break;
    }

    default:
        break;
    }
}

void App::RecalculateScale()
{
    Vector2 scaleFactor{};
    Vector2 screen1Pos{};
    Vector2 screen2Pos{};
    
    auto bounds = ApplicationView::GetForCurrentView()->VisibleBounds;
    auto rawScaleFactor = DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;

    float width = bounds.Width * rawScaleFactor;
    float height = bounds.Height * rawScaleFactor;

    if (Config::ScreenLayout == 2)
    {
        scaleFactor = Vector2{ width / 256 / 2 };

        screen1Pos = { 0.0f };
        screen2Pos = { scaleFactor.x * 256, 0.0f };
    }
    else if (Config::ScreenLayout == 0)
    {
        scaleFactor = Vector2{ height / 192 / 2 };

        if (scaleFactor.y * 256 > width)
        {
            scaleFactor = Vector2{ width / 256 };
        }

        screen1Pos = { max((width - 256 * scaleFactor.y) / 2, 0), 0 };
        screen2Pos = { max((width - 256 * scaleFactor.y) / 2 , 0), scaleFactor.y * 192 };
    }

    m_emulator->SetScale(scaleFactor);
    m_emulator->SetScreen1Pos(screen1Pos);
    m_emulator->SetScreen2Pos(screen2Pos);
}

void App::OnWindowSizeChange(Platform::Object ^sender, WindowSizeChangedEventArgs ^e)
{
    RecalculateScale();
}