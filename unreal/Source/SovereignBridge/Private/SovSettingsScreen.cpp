#include "SovSettingsScreen.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Widgets/SWindow.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const TCHAR* const kSection = TEXT("Sovereign.Interface");
const TCHAR* const kQualities[] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic")};
const TCHAR* const kModes[] = {TEXT("Fullscreen"), TEXT("Windowed fullscreen"), TEXT("Windowed")};
const int32 kLimits[] = {0, 30, 60, 120, 144, 240};  // 0: no limit

// The controls, for reference (the README's table has the details).
const TCHAR* const kControls[][2] = {
	{TEXT("Left click"), TEXT("Select a unit or city; click again to cycle")},
	{TEXT("Right click"), TEXT("Move or attack with the selected unit; city strike")},
	{TEXT("Space / Enter"), TEXT("End the turn (or open what blocks it)")},
	{TEXT("Esc"), TEXT("Close a list, clear the selection, then the menu")},
	{TEXT("WASD / arrows, wheel, Home"), TEXT("Pan, zoom, back to your capital")},
	{TEXT("."), TEXT("Next unit needing orders")},
	{TEXT("F"), TEXT("Found a city; use a great person; trade route; talk in the street")},
	{TEXT("B"), TEXT("Builder: improve the plot; a battle: fight it live")},
	{TEXT("K / G"), TEXT("Skip the unit / fortify or sleep")},
	{TEXT("U"), TEXT("Promote the unit or leader")},
	{TEXT("E / L / H"), TEXT("Leader's gear / escort link / the throne")},
	{TEXT("Q"), TEXT("Walk the City Center with your leader")},
	{TEXT("V / X"), TEXT("Benevolence / Fear in the leader's city")},
	{TEXT("P / T / C"), TEXT("Production / research tree / civics tree")},
	{TEXT("F2"), TEXT("Government, policies and dedications")},
	{TEXT("Y / Z / J"), TEXT("Great people / governors / agents")},
	{TEXT("I"), TEXT("Pantheon")},
	{TEXT("N / O / ,"), TEXT("Diplomacy / city-states / World Congress")},
	{TEXT("1-9, 0"), TEXT("Pick from an open list, next page")},
	{TEXT("Shift+click"), TEXT("With a city selected: lock or free a citizen")},
	{TEXT("F1 / F3"), TEXT("How to play / plot yields")},
	{TEXT("F4 / F6"), TEXT("The chronicle / have it written up")},
	{TEXT("F5 / F9"), TEXT("Quicksave / quickload")},
	{TEXT("M"), TEXT("Chat (online)")}};

void SetInterfaceScale(float Scale)
{
	if (UUserInterfaceSettings* UI = GetMutableDefault<UUserInterfaceSettings>()) UI->ApplicationScale = Scale;
}
}  // namespace

void SSovSettingsScreen::ApplySavedInterfaceScale()
{
	float Saved = 1.f;
	if (GConfig && GConfig->GetFloat(kSection, TEXT("Scale"), Saved, GGameUserSettingsIni)) SetInterfaceScale(FMath::Clamp(Saved, 0.7f, 1.6f));
}

void SSovSettingsScreen::Construct(const FArguments& Args)
{
	OnBack = Args._OnBack;
	// Start from what is in effect now.
	if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings())
	{
		Quality = FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 4);
		if (S->GetOverallScalabilityLevel() < 0) Quality = 3;  // custom: show Epic
		WindowMode = FMath::Clamp(static_cast<int32>(S->GetFullscreenMode()), 0, 2);
		// The window as it is now (the saved setting can differ when the game was started with -windowed).
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
			WindowMode = FMath::Clamp(static_cast<int32>(GEngine->GameViewport->GetWindow()->GetWindowMode()), 0, 2);
		bVSync = S->IsVSyncEnabled();
		const float Limit = S->GetFrameRateLimit();
		for (int32 i = 0; i < UE_ARRAY_COUNT(kLimits); ++i)
			if (FMath::IsNearlyEqual(Limit, static_cast<float>(kLimits[i]))) FrameLimit = i;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		const FIntPoint Now = S->GetScreenResolution();
		Resolutions.AddUnique(Now);
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		Resolution = Resolutions.IndexOfByKey(Now);
	}
	if (const UUserInterfaceSettings* UI = GetDefault<UUserInterfaceSettings>()) Scale = FMath::Clamp(FMath::RoundToInt(UI->ApplicationScale * 10.f), 7, 16);

	TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Options->AddSlot().AutoHeight().Padding(0, 3)[W]; };
	Add(Option(TEXT("Graphics quality"), [this]() { return FString(kQualities[Quality]); }, [this](int32 D) { Quality = FMath::Clamp(Quality + D, 0, 4); }));
	Add(Option(TEXT("Window"), [this]() { return FString(kModes[WindowMode]); }, [this](int32 D) { WindowMode = (WindowMode + D + 3) % 3; }));
	Add(Option(TEXT("Resolution"), [this]() {
		return Resolutions.IsValidIndex(Resolution) ? FString::Printf(TEXT("%d x %d"), Resolutions[Resolution].X, Resolutions[Resolution].Y) : FString(TEXT("-"));
	}, [this](int32 D) {
		if (Resolutions.Num() > 0) Resolution = FMath::Clamp(Resolution + D, 0, Resolutions.Num() - 1);
	}));
	Add(Option(TEXT("Vertical sync"), [this]() { return FString(bVSync ? TEXT("On") : TEXT("Off")); }, [this](int32) { bVSync = !bVSync; }));
	Add(Option(TEXT("Frame limit"), [this]() { return kLimits[FrameLimit] ? FString::Printf(TEXT("%d fps"), kLimits[FrameLimit]) : FString(TEXT("None")); },
		[this](int32 D) { FrameLimit = FMath::Clamp(FrameLimit + D, 0, static_cast<int32>(UE_ARRAY_COUNT(kLimits)) - 1); }));
	Add(Option(TEXT("Interface scale"), [this]() { return FString::Printf(TEXT("%d%%"), Scale * 10); }, [this](int32 D) { Scale = FMath::Clamp(Scale + D, 7, 16); }));
	Options->AddSlot().AutoHeight().Padding(0, 10, 0, 0)
	[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true)
		.Text(FText::FromString(TEXT("Sound: none yet. The keys are fixed for now; rebinding comes later.")))];

	TSharedRef<SVerticalBox> Keys = SNew(SVerticalBox);
	for (const auto& Row : kControls)
	{
		Keys->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SBox).WidthOverride(190)[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(Row[0]))]]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(Row[1]))]
		];
	}

	auto Button = [](const TCHAR* Label, const FButtonStyle* Style, TFunction<void()> Click) {
		return SNew(SButton).IsFocusable(false).ButtonStyle(Style).ContentPadding(FMargin(18, 6)).OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
			[SNew(STextBlock).Font(FSovStyle::Font(13, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(Label))];
	};
	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f, 0.014f, 0.013f, 0.97f)).Padding(0)
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1060.f).HeightOverride(700.f)
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(18, 14))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[SNew(STextBlock).Font(FSovStyle::Font(24, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(TEXT("Settings")))]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(400.f)[Options]]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(24, 0, 0, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
							[SNew(STextBlock).Font(FSovStyle::Font(13, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Controls")))]
							+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Keys]]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Back"), &FSovStyle::Button(), [this]() { OnBack.ExecuteIfBound(); })]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16, 0)
						[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this]() { return FText::FromString(Status); })]
						+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Apply"), &FSovStyle::Primary(), [this]() { Apply(); })]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SSovSettingsScreen::Option(const FString& Label, TFunction<FString()> Value, TFunction<void(int32)> Step)
{
	auto Arrow = [Step](const TCHAR* Text, int32 D) {
		return SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).OnClicked_Lambda([Step, D]() { Step(D); return FReply::Handled(); })
			[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(Text))];
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(Label))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("<"), -1)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[SNew(STextBlock).Font(FSovStyle::Font(12)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([Value]() { return FText::FromString(Value()); })]
			+ SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT(">"), 1)]
		];
}

void SSovSettingsScreen::Apply()
{
	if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings())
	{
		S->SetOverallScalabilityLevel(Quality);
		S->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowMode));
		if (Resolutions.IsValidIndex(Resolution)) S->SetScreenResolution(Resolutions[Resolution]);
		S->SetVSyncEnabled(bVSync);
		S->SetFrameRateLimit(static_cast<float>(kLimits[FrameLimit]));
		S->ApplySettings(false);  // also saves them
	}
	SetInterfaceScale(Scale / 10.f);
	if (GConfig)
	{
		GConfig->SetFloat(kSection, TEXT("Scale"), Scale / 10.f, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	Status = TEXT("Applied and saved.");
}
