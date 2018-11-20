#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

namespace uwp
{
#pragma region button positions (in cm based on 6x10cm phone)
const double PadLeftPDefault = 0.1f; //from left
const double PadBottomPDefault = 0.9f; //from bottom
const double ACenterXPDefault = 0.6f;  //from right
const double ACenterYPDefault = 2.8f;  //from bottom
const double BCenterXPDefault = 1.7f; //from right
const double BCenterYPDefault = 1.8f; //from bottom
const double startCenterXPDefault = 0.85f; //from center
const double startBottomPDefault = 0.1f;  //from bottom
const double selectCenterXPDefault = -0.85f;  //from center
const double selectBottomPDefault = 0.1f;  //from bottom
const double LLeftPDefault = 0.0f;  //from left
const double LCenterYPDefault = 4.0f;  ///from bottom
const double RRightPDefault = 0.0f;  //from right
const double RCenterYPDefault = 4.0f;  //from bottom
const double XCenterXPDefault = 1.8f;  //from right
const double XCenterYPDefault = 2.8f; //from bottom
const double YCenterXPDefault = 0.6f;  //from right
const double YCenterYPDefault = 1.8f; //from bottom

const double PadLeftLDefault = 0.3f; //from left
const double PadBottomLDefault = 0.3f; //from bottom
const double ACenterXLDefault = 0.8f; //from right
const double ACenterYLDefault = 2.1f; //from bottom
const double BCenterXLDefault = 2.0f; //from right
const double BCenterYLDefault = 1.0f; //from bottom
const double startCenterXLDefault = 1.0f;
const double startBottomLDefault = 0.3f;
const double selectCenterXLDefault = -1.0f;
const double selectBottomLDefault = 0.3f;
const double LLeftLDefault = 0.0f;
const double LCenterYLDefault = 3.8f;
const double RRightLDefault = 0.0f;
const double RCenterYLDefault = 3.8f;
const double XCenterXLDefault = 2.0f;
const double XCenterYLDefault = 2.1f;
const double YCenterXLDefault = 0.8f;
const double YCenterYLDefault = 1.1f;
#pragma endregion

#define DECLARE_CONFIG(name, type, default_val)         \
property type name                                      \
{                                                       \
    type get()                                          \
    {                                                   \
        return GetValueOrDefault(#name, default_val);  \
    }                                                   \
    void set(type val)                                  \
    {                                                   \
        localSettings->Values->Insert(#name, val);     \
    }                                                   \
}


ref class EmulatorConfig sealed
{
public:
    DECLARE_CONFIG(EnableThreaded3D, bool, true)
    DECLARE_CONFIG(VirtualControllerScale, double, 1.0f)

    DECLARE_CONFIG(StartXL, double, startCenterXLDefault)
    DECLARE_CONFIG(StartXP, double, startCenterXPDefault)
    DECLARE_CONFIG(StartYL, double, startBottomLDefault)
    DECLARE_CONFIG(StartYP, double, startBottomPDefault)

    DECLARE_CONFIG(SelectXL, double, selectCenterXLDefault)
    DECLARE_CONFIG(SelectXP, double, selectCenterXPDefault)
    DECLARE_CONFIG(SelectYL, double, selectBottomLDefault)
    DECLARE_CONFIG(SelectYP, double, selectBottomPDefault)

    DECLARE_CONFIG(LeftXL, double, LLeftLDefault)
    DECLARE_CONFIG(LeftXP, double, LLeftPDefault)
    DECLARE_CONFIG(LeftYL, double, LCenterYLDefault)
    DECLARE_CONFIG(LeftYP, double, LCenterYPDefault)

    DECLARE_CONFIG(RightXL, double, RRightLDefault)
    DECLARE_CONFIG(RightXP, double, RRightPDefault)
    DECLARE_CONFIG(RightYL, double, RCenterYLDefault)
    DECLARE_CONFIG(RightYP, double, RCenterYPDefault)

    DECLARE_CONFIG(PadLeftL, double, PadLeftLDefault)
    DECLARE_CONFIG(PadBottomL, double, PadBottomLDefault)

    DECLARE_CONFIG(PadLeftP, double, PadLeftPDefault)
    DECLARE_CONFIG(PadBottomP, double, PadBottomPDefault)

    DECLARE_CONFIG(ACenterXL, double, ACenterXLDefault)
    DECLARE_CONFIG(ACenterYL, double, ACenterYLDefault)

    DECLARE_CONFIG(ACenterXP, double, ACenterXPDefault)
    DECLARE_CONFIG(ACenterYP, double, ACenterYPDefault)

    DECLARE_CONFIG(BCenterXL, double, BCenterXLDefault)
    DECLARE_CONFIG(BCenterYL, double, BCenterYLDefault)

    DECLARE_CONFIG(BCenterXP, double, BCenterXPDefault)
    DECLARE_CONFIG(BCenterYP, double, BCenterYPDefault)

    DECLARE_CONFIG(XCenterXL, double, XCenterXLDefault)
    DECLARE_CONFIG(XCenterYL, double, XCenterYLDefault)

    DECLARE_CONFIG(XCenterXP, double, XCenterXPDefault)
    DECLARE_CONFIG(XCenterYP, double, XCenterYPDefault)

    DECLARE_CONFIG(YCenterXL, double, YCenterXLDefault)
    DECLARE_CONFIG(YCenterYL, double, YCenterYLDefault)

    DECLARE_CONFIG(YCenterXP, double, YCenterXPDefault)
    DECLARE_CONFIG(YCenterYP, double, YCenterYPDefault)

    EmulatorConfig()
    {
        localSettings = Windows::Storage::ApplicationData::Current->LocalSettings;
    }

private:
	template<typename T>
	T GetValueOrDefault(Platform::String^ key, T defaultValue)
	{

		if (localSettings->Values->HasKey(key))
		{
			return (T)localSettings->Values->Lookup(key);
		}
		else
		{
			return defaultValue;
		}
	}

    Windows::Storage::ApplicationDataContainer^ localSettings;
};

}