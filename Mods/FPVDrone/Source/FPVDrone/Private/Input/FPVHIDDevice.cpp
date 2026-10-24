#include "Input/FPVHIDDevice.h"

#include "FPVDroneModule.h"
#include "FPVSettings.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

#include <atomic>

#if FPV_WITH_HID && PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <setupapi.h>
#include <hidsdi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#define FPV_HID_ACTIVE 1
#else
#define FPV_HID_ACTIVE 0
#endif

#if FPV_HID_ACTIVE

namespace FPVHID
{
	constexpr USHORT UsagePageGenericDesktop = 0x01;
	constexpr USHORT UsagePageSimulation = 0x02;
	constexpr USHORT UsagePageButton = 0x09;
	constexpr USHORT UsageJoystick = 0x04;
	constexpr USHORT UsageGamepad = 0x05;
	constexpr USHORT UsageMultiAxis = 0x08;
	constexpr USHORT UsageHatSwitch = 0x39;

	struct FAxisCap
	{
		USHORT UsagePage = 0;
		USHORT Usage = 0;
		USHORT LinkCollection = 0;
		LONG LogicalMin = 0;
		LONG LogicalMax = 0;
		USHORT BitSize = 0;
	};

	FString UsageName(USHORT Page, USHORT Usage)
	{
		if (Page == UsagePageGenericDesktop)
		{
			switch (Usage)
			{
			case 0x30: return TEXT("X");
			case 0x31: return TEXT("Y");
			case 0x32: return TEXT("Z");
			case 0x33: return TEXT("Rx");
			case 0x34: return TEXT("Ry");
			case 0x35: return TEXT("Rz");
			case 0x36: return TEXT("Slider");
			case 0x37: return TEXT("Dial");
			case 0x38: return TEXT("Wheel");
			default: break;
			}
		}
		if (Page == UsagePageSimulation)
		{
			switch (Usage)
			{
			case 0xBA: return TEXT("Rudder");
			case 0xBB: return TEXT("Throttle");
			default: break;
			}
		}
		return FString::Printf(TEXT("0x%02X:%02X"), Page, Usage);
	}

	struct FOpenDevice
	{
		HANDLE Handle = INVALID_HANDLE_VALUE;
		PHIDP_PREPARSED_DATA Preparsed = nullptr;
		HIDP_CAPS Caps = {};
		TArray<FAxisCap> Axes;
		USHORT ButtonUsageMin = 1;
		int32 NumButtons = 0;

		void Close()
		{
			if (Preparsed != nullptr)
			{
				HidD_FreePreparsedData(Preparsed);
				Preparsed = nullptr;
			}
			if (Handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(Handle);
				Handle = INVALID_HANDLE_VALUE;
			}
		}
	};

	bool ReadCaps(HANDLE Handle, FOpenDevice& Out)
	{
		if (!HidD_GetPreparsedData(Handle, &Out.Preparsed))
		{
			return false;
		}
		if (HidP_GetCaps(Out.Preparsed, &Out.Caps) != HIDP_STATUS_SUCCESS)
		{
			return false;
		}

		USHORT NumValueCaps = Out.Caps.NumberInputValueCaps;
		TArray<HIDP_VALUE_CAPS> ValueCaps;
		ValueCaps.SetNumZeroed(NumValueCaps);
		if (NumValueCaps > 0 && HidP_GetValueCaps(HidP_Input, ValueCaps.GetData(), &NumValueCaps, Out.Preparsed) == HIDP_STATUS_SUCCESS)
		{
			for (int32 Index = 0; Index < NumValueCaps; ++Index)
			{
				const HIDP_VALUE_CAPS& Cap = ValueCaps[Index];
				const USHORT First = Cap.IsRange ? Cap.Range.UsageMin : Cap.NotRange.Usage;
				const USHORT Last = Cap.IsRange ? Cap.Range.UsageMax : Cap.NotRange.Usage;
				for (USHORT Usage = First; Usage <= Last; ++Usage)
				{
					if (Cap.UsagePage == UsagePageGenericDesktop && Usage == UsageHatSwitch)
					{
						continue;
					}
					FAxisCap Axis;
					Axis.UsagePage = Cap.UsagePage;
					Axis.Usage = Usage;
					Axis.LinkCollection = Cap.LinkCollection;
					Axis.LogicalMin = Cap.LogicalMin;
					Axis.LogicalMax = Cap.LogicalMax;
					Axis.BitSize = Cap.BitSize;
					Out.Axes.Add(Axis);
					if (Usage == 0xFFFF)
					{
						break;
					}
				}
			}
		}
		// X, Y, Z, Rx, Ry, Rz, Slider, Dial: matches the channel order EdgeTX/OpenTX use.
		Out.Axes.StableSort([](const FAxisCap& A, const FAxisCap& B)
		{
			return A.UsagePage != B.UsagePage ? A.UsagePage < B.UsagePage : A.Usage < B.Usage;
		});

		USHORT NumButtonCaps = Out.Caps.NumberInputButtonCaps;
		TArray<HIDP_BUTTON_CAPS> ButtonCaps;
		ButtonCaps.SetNumZeroed(NumButtonCaps);
		if (NumButtonCaps > 0 && HidP_GetButtonCaps(HidP_Input, ButtonCaps.GetData(), &NumButtonCaps, Out.Preparsed) == HIDP_STATUS_SUCCESS)
		{
			USHORT MinUsage = 0xFFFF, MaxUsage = 0;
			for (int32 Index = 0; Index < NumButtonCaps; ++Index)
			{
				const HIDP_BUTTON_CAPS& Cap = ButtonCaps[Index];
				if (Cap.UsagePage != UsagePageButton)
				{
					continue;
				}
				MinUsage = FMath::Min<USHORT>(MinUsage, Cap.IsRange ? Cap.Range.UsageMin : Cap.NotRange.Usage);
				MaxUsage = FMath::Max<USHORT>(MaxUsage, Cap.IsRange ? Cap.Range.UsageMax : Cap.NotRange.Usage);
			}
			if (MaxUsage >= MinUsage && MinUsage != 0xFFFF)
			{
				Out.ButtonUsageMin = MinUsage;
				Out.NumButtons = FMath::Min<int32>(MaxUsage - MinUsage + 1, 128);
			}
		}
		return true;
	}

	float NormalizeAxis(const FAxisCap& Cap, ULONG Raw)
	{
		int64 Min = Cap.LogicalMin;
		int64 Max = Cap.LogicalMax;
		int64 Value = static_cast<int64>(Raw);

		if (Max <= Min)
		{
			// Some descriptors declare an unsigned range that does not fit in a signed LONG.
			Min = 0;
			Max = (Cap.BitSize >= 32) ? 0xFFFFFFFFll : ((1ll << Cap.BitSize) - 1);
		}
		else if (Min < 0 && Cap.BitSize > 0 && Cap.BitSize < 32)
		{
			// Sign-extend.
			const int64 SignBit = 1ll << (Cap.BitSize - 1);
			Value = (Value ^ SignBit) - SignBit;
		}
		const double T = static_cast<double>(Value - Min) / static_cast<double>(Max - Min);
		return FMath::Clamp(static_cast<float>(T * 2.0 - 1.0), -1.0f, 1.0f);
	}

	FString ProductName(HANDLE Handle, const FString& Fallback)
	{
		WCHAR Buffer[256] = {};
		if (HidD_GetProductString(Handle, Buffer, sizeof(Buffer)) && Buffer[0] != 0)
		{
			return FString(Buffer).TrimStartAndEnd();
		}
		return Fallback;
	}

	TArray<FFPVHIDDeviceInfo> Enumerate()
	{
		TArray<FFPVHIDDeviceInfo> Result;

		GUID HidGuid;
		HidD_GetHidGuid(&HidGuid);
		HDEVINFO Info = SetupDiGetClassDevsW(&HidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (Info == INVALID_HANDLE_VALUE)
		{
			return Result;
		}

		SP_DEVICE_INTERFACE_DATA InterfaceData;
		InterfaceData.cbSize = sizeof(InterfaceData);
		for (DWORD Index = 0; SetupDiEnumDeviceInterfaces(Info, nullptr, &HidGuid, Index, &InterfaceData); ++Index)
		{
			DWORD Size = 0;
			SetupDiGetDeviceInterfaceDetailW(Info, &InterfaceData, nullptr, 0, &Size, nullptr);
			if (Size == 0)
			{
				continue;
			}
			TArray<uint8> Buffer;
			Buffer.SetNumZeroed(Size);
			PSP_DEVICE_INTERFACE_DETAIL_DATA_W Detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(Buffer.GetData());
			Detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
			if (!SetupDiGetDeviceInterfaceDetailW(Info, &InterfaceData, Detail, Size, nullptr, nullptr))
			{
				continue;
			}

			// Access 0 = query only, works even if another program has the device open.
			HANDLE Handle = CreateFileW(Detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			FOpenDevice Probe;
			Probe.Handle = Handle;
			if (ReadCaps(Handle, Probe) && Probe.Caps.UsagePage == UsagePageGenericDesktop &&
				(Probe.Caps.Usage == UsageJoystick || Probe.Caps.Usage == UsageGamepad || Probe.Caps.Usage == UsageMultiAxis) &&
				Probe.Axes.Num() > 0)
			{
				FFPVHIDDeviceInfo Device;
				Device.Path = Detail->DevicePath;
				Device.Name = ProductName(Handle, FString::Printf(TEXT("HID Joystick %d"), Result.Num() + 1));
				Device.NumAxes = Probe.Axes.Num();
				Device.NumButtons = Probe.NumButtons;
				Device.bIsJoystick = Probe.Caps.Usage != UsageGamepad;
				Result.Add(Device);
			}
			Probe.Close();
		}
		SetupDiDestroyDeviceInfoList(Info);
		return Result;
	}
}

struct FFPVHIDManager::FImpl : public FRunnable
{
	mutable FCriticalSection Lock;
	TArray<FFPVHIDDeviceInfo> Devices;
	FFPVHIDSnapshot Snapshot;

	FPVHID::FOpenDevice Device;
	FRunnableThread* Thread = nullptr;
	HANDLE StopEvent = nullptr;
	std::atomic<bool> bStopRequested { false };
	std::atomic<bool> bThreadRunning { false };

	bool Open(const FFPVHIDDeviceInfo& Info)
	{
		Close();

		HANDLE Handle = CreateFileW(*Info.Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (Handle == INVALID_HANDLE_VALUE)
		{
			UE_LOG(LogFPVDrone, Warning, TEXT("HID: cannot open %s (error %u)"), *Info.Name, GetLastError());
			return false;
		}
		Device.Handle = Handle;
		if (!FPVHID::ReadCaps(Handle, Device))
		{
			Device.Close();
			return false;
		}
		// Ask the driver for a deeper buffer so no report gets lost between reads.
		HidD_SetNumInputBuffers(Handle, 64);

		{
			FScopeLock ScopeLock(&Lock);
			Snapshot = FFPVHIDSnapshot();
			Snapshot.bConnected = true;
			Snapshot.DeviceName = Info.Name;
			Snapshot.Axes.SetNumZeroed(Device.Axes.Num());
			Snapshot.Buttons.SetNumZeroed(Device.NumButtons);
			for (const FPVHID::FAxisCap& Cap : Device.Axes)
			{
				Snapshot.AxisNames.Add(FPVHID::UsageName(Cap.UsagePage, Cap.Usage));
			}
		}

		UE_LOG(LogFPVDrone, Log, TEXT("HID: opened %s (%d axes, %d buttons)"), *Info.Name, Device.Axes.Num(), Device.NumButtons);

		bStopRequested = false;
		StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		bThreadRunning = true;
		Thread = FRunnableThread::Create(this, TEXT("FPVDroneHIDReader"), 0, TPri_AboveNormal);
		return Thread != nullptr;
	}

	void Close()
	{
		if (Thread != nullptr)
		{
			bStopRequested = true;
			if (StopEvent != nullptr)
			{
				SetEvent(StopEvent);
			}
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
		if (StopEvent != nullptr)
		{
			CloseHandle(StopEvent);
			StopEvent = nullptr;
		}
		Device.Close();
		FScopeLock ScopeLock(&Lock);
		Snapshot.bConnected = false;
	}

	virtual uint32 Run() override
	{
		const ULONG ReportLength = Device.Caps.InputReportByteLength;
		TArray<uint8> Report;
		Report.SetNumZeroed(ReportLength);
		TArray<USAGE> Pressed;
		Pressed.SetNumZeroed(FMath::Max(1, Device.NumButtons));

		OVERLAPPED Overlapped = {};
		Overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		HANDLE WaitHandles[2] = { Overlapped.hEvent, StopEvent };

		while (!bStopRequested)
		{
			ResetEvent(Overlapped.hEvent);
			DWORD BytesRead = 0;
			if (!ReadFile(Device.Handle, Report.GetData(), ReportLength, nullptr, &Overlapped))
			{
				if (GetLastError() != ERROR_IO_PENDING)
				{
					break; // device unplugged
				}
				const DWORD Wait = WaitForMultipleObjects(2, WaitHandles, FALSE, 250);
				if (Wait != WAIT_OBJECT_0)
				{
					CancelIo(Device.Handle);
					GetOverlappedResult(Device.Handle, &Overlapped, &BytesRead, TRUE);
					if (Wait == WAIT_TIMEOUT)
					{
						continue; // radios only send on change, this is normal
					}
					break; // stop requested
				}
			}
			if (!GetOverlappedResult(Device.Handle, &Overlapped, &BytesRead, FALSE) || BytesRead == 0)
			{
				break;
			}

			FScopeLock ScopeLock(&Lock);
			for (int32 Index = 0; Index < Device.Axes.Num(); ++Index)
			{
				const FPVHID::FAxisCap& Cap = Device.Axes[Index];
				ULONG Value = 0;
				if (HidP_GetUsageValue(HidP_Input, Cap.UsagePage, Cap.LinkCollection, Cap.Usage, &Value, Device.Preparsed,
					reinterpret_cast<PCHAR>(Report.GetData()), ReportLength) == HIDP_STATUS_SUCCESS)
				{
					Snapshot.Axes[Index] = FPVHID::NormalizeAxis(Cap, Value);
				}
			}
			if (Device.NumButtons > 0)
			{
				ULONG NumPressed = static_cast<ULONG>(Pressed.Num());
				if (HidP_GetUsages(HidP_Input, FPVHID::UsagePageButton, 0, Pressed.GetData(), &NumPressed, Device.Preparsed,
					reinterpret_cast<PCHAR>(Report.GetData()), ReportLength) == HIDP_STATUS_SUCCESS)
				{
					for (bool& Button : Snapshot.Buttons) { Button = false; }
					for (ULONG Index = 0; Index < NumPressed; ++Index)
					{
						const int32 Button = static_cast<int32>(Pressed[Index]) - Device.ButtonUsageMin;
						if (Snapshot.Buttons.IsValidIndex(Button))
						{
							Snapshot.Buttons[Button] = true;
						}
					}
				}
			}
			Snapshot.LastReportTime = FPlatformTime::Seconds();
			++Snapshot.ReportCount;
		}

		CloseHandle(Overlapped.hEvent);
		{
			FScopeLock ScopeLock(&Lock);
			Snapshot.bConnected = false;
		}
		bThreadRunning = false;
		return 0;
	}
};

#else

struct FFPVHIDManager::FImpl
{
	mutable FCriticalSection Lock;
	TArray<FFPVHIDDeviceInfo> Devices;
	FFPVHIDSnapshot Snapshot;
	std::atomic<bool> bThreadRunning { false };

	bool Open(const FFPVHIDDeviceInfo&) { return false; }
	void Close() {}
};

#endif

FFPVHIDManager& FFPVHIDManager::Get()
{
	static FFPVHIDManager Instance;
	return Instance;
}

FFPVHIDManager::~FFPVHIDManager()
{
	Stop();
}

bool FFPVHIDManager::IsSupported() const
{
	return FPV_HID_ACTIVE != 0;
}

void FFPVHIDManager::Start()
{
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	Impl = MakeUnique<FImpl>();
	RefreshDevices();
	SelectDevice(FFPVSettings::Get().Radio.DeviceName);
}

void FFPVHIDManager::Stop()
{
	if (Impl.IsValid())
	{
		Impl->Close();
		Impl.Reset();
	}
	bStarted = false;
}

void FFPVHIDManager::Tick()
{
	if (!Impl.IsValid() || Impl->bThreadRunning)
	{
		return;
	}
	const EFPVInputSource Source = FFPVSettings::Get().Radio.Source;
	if (Source == EFPVInputSource::Gamepad || Source == EFPVInputSource::Keyboard)
	{
		return;
	}
	// Device lost or never found: retry every few seconds (hot-plug).
	const double Now = FPlatformTime::Seconds();
	if (Now - LastReconnectAttempt > 5.0)
	{
		LastReconnectAttempt = Now;
		RefreshDevices();
		SelectDevice(FFPVSettings::Get().Radio.DeviceName);
	}
}

void FFPVHIDManager::RefreshDevices()
{
	if (!Impl.IsValid())
	{
		return;
	}
#if FPV_HID_ACTIVE
	TArray<FFPVHIDDeviceInfo> Found = FPVHID::Enumerate();
	FScopeLock ScopeLock(&Impl->Lock);
	Impl->Devices = MoveTemp(Found);
#endif
}

TArray<FFPVHIDDeviceInfo> FFPVHIDManager::GetDevices() const
{
	if (!Impl.IsValid())
	{
		return {};
	}
	FScopeLock ScopeLock(&Impl->Lock);
	return Impl->Devices;
}

void FFPVHIDManager::SelectDevice(const FString& Name)
{
	if (!Impl.IsValid())
	{
		return;
	}
	const TArray<FFPVHIDDeviceInfo> Devices = GetDevices();
	const FFPVHIDDeviceInfo* Chosen = nullptr;
	if (!Name.IsEmpty())
	{
		Chosen = Devices.FindByPredicate([&](const FFPVHIDDeviceInfo& D) { return D.Name == Name; });
	}
	if (Chosen == nullptr)
	{
		// Prefer things that look like a radio, then any joystick, then any gamepad.
		static const TCHAR* RadioHints[] = { TEXT("radiomaster"), TEXT("edgetx"), TEXT("opentx"), TEXT("jumper"), TEXT("tbs"), TEXT("frsky"), TEXT("betafpv"), TEXT("radio") };
		for (const FFPVHIDDeviceInfo& D : Devices)
		{
			for (const TCHAR* Hint : RadioHints)
			{
				if (D.Name.Contains(Hint, ESearchCase::IgnoreCase))
				{
					Chosen = &D;
					break;
				}
			}
			if (Chosen != nullptr)
			{
				break;
			}
		}
		if (Chosen == nullptr && Name.IsEmpty())
		{
			Chosen = Devices.FindByPredicate([](const FFPVHIDDeviceInfo& D) { return D.bIsJoystick; });
		}
	}
	if (Chosen != nullptr)
	{
		Impl->Open(*Chosen);
	}
	else
	{
		Impl->Close();
	}
}

bool FFPVHIDManager::GetSnapshot(FFPVHIDSnapshot& Out) const
{
	if (!Impl.IsValid())
	{
		return false;
	}
	FScopeLock ScopeLock(&Impl->Lock);
	Out = Impl->Snapshot;
	return Out.bConnected;
}
