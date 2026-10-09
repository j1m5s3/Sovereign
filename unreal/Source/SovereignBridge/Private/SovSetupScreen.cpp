#include "SovSetupScreen.h"

#include "SovStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "sovereign/rules.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

struct FChoice
{
	const TCHAR* Id;
	const TCHAR* Label;
};
const FChoice kSpeeds[] = {{TEXT("GAMESPEED_SHORT_REIGN"), TEXT("Short Reign (100 turns)")}, {TEXT("GAMESPEED_ONLINE"), TEXT("Online (250 turns)")},
	{TEXT("GAMESPEED_QUICK"), TEXT("Quick (330 turns)")}, {TEXT("GAMESPEED_STANDARD"), TEXT("Standard (500 turns)")}, {TEXT("GAMESPEED_EPIC"), TEXT("Epic (750 turns)")},
	{TEXT("GAMESPEED_MARATHON"), TEXT("Marathon (1500 turns)")}};
const FChoice kEras[] = {{TEXT(""), TEXT("Ancient")}, {TEXT("ERA_CLASSICAL"), TEXT("Classical")}, {TEXT("ERA_MEDIEVAL"), TEXT("Medieval")},
	{TEXT("ERA_RENAISSANCE"), TEXT("Renaissance")}, {TEXT("ERA_INDUSTRIAL"), TEXT("Industrial")}, {TEXT("ERA_MODERN"), TEXT("Modern")}, {TEXT("ERA_ATOMIC"), TEXT("Atomic")},
	{TEXT("ERA_INFORMATION"), TEXT("Information")}};
const TCHAR* const kLevels[] = {TEXT("Settler"), TEXT("Chieftain"), TEXT("Warlord"), TEXT("Prince"), TEXT("King"), TEXT("Emperor"), TEXT("Immortal"), TEXT("Deity")};
const TCHAR* const kDisasters[] = {TEXT("None"), TEXT("Minimal"), TEXT("Light"), TEXT("Moderate"), TEXT("Heavy"), TEXT("Hyperreal")};  // -1 .. 4

template <int32 N>
int32 IndexOf(const FChoice (&List)[N], const FString& Id)
{
	for (int32 i = 0; i < N; ++i)
		if (Id == List[i].Id) return i;
	return 0;
}

// "MAPSIZE_STANDARD" -> "Standard".
FString Pretty(const std::string& Id)
{
	FString S = Str(Id);
	int32 Cut;
	if (S.FindChar(TEXT('_'), Cut)) S = S.RightChop(Cut + 1);
	S = S.Replace(TEXT("_"), TEXT(" ")).ToLower();
	if (!S.IsEmpty()) S[0] = FChar::ToUpper(S[0]);
	return S;
}

FString LeaderName(const sov::Rules& R, int32 Civ)
{
	if (const sov::Dynasty* D = R.dynastyOf(static_cast<sov::TypeIndex>(Civ)); D && !D->names.empty()) return Str(D->names.front());
	return Pretty(R.civs[static_cast<size_t>(Civ)].leader);
}
}  // namespace

void SSovSetupScreen::Construct(const FArguments& Args)
{
	Rules = Args._Rules;
	Setup = Args._Setup;
	OnStart = Args._OnStart;
	OnBack = Args._OnBack;
	check(Rules.IsValid());
	const sov::Rules& R = *Rules;
	if (!Setup.Civ.IsEmpty()) CivPick = R.civ(TCHAR_TO_UTF8(*Setup.Civ));
	if (Setup.Seed == 7 || Setup.Seed == 0) Setup.Seed = FPlatformTime::Cycles64() % 999999 + 1;  // a new map each time

	// The civs: Random first, then the roster.
	TSharedRef<SVerticalBox> Civs = SNew(SVerticalBox);
	for (int32 i = -1; i < static_cast<int32>(R.civs.size()); ++i)
	{
		const FString Name = i < 0 ? FString(TEXT("Random")) : Str(R.civs[static_cast<size_t>(i)].name);
		const FString Sub = i < 0 ? FString(TEXT("A civ chosen for you")) : LeaderName(R, i) + TEXT(" - ") + Str(R.civs[static_cast<size_t>(i)].leaning);
		Civs->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button())
			.ButtonColorAndOpacity_Lambda([this, i]() { return CivPick == i ? FSovStyle::Gold : FLinearColor::White; })
			.OnClicked_Lambda([this, i]() { CivPick = i; return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(Name))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(Sub))]
			]
		];
	}

	// The options: each a label, its value and buttons to step it.
	TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Options->AddSlot().AutoHeight().Padding(0, 3)[W]; };
	Add(Option(TEXT("Map"), [this]() {
		const sov::TypeIndex M = Rules->mapSize(TCHAR_TO_UTF8(*Setup.MapSize));
		if (M == sov::kNone) return Setup.MapSize;
		const sov::MapSizeType& T = Rules->mapSizes[static_cast<size_t>(M)];
		return FString::Printf(TEXT("%s (%d x %d)"), *Pretty(T.id), T.width, T.height);
	}, [this](int32 D) {
		const int32 N = static_cast<int32>(Rules->mapSizes.size());
		if (N == 0) return;
		const int32 At = FMath::Max(0, static_cast<int32>(Rules->mapSize(TCHAR_TO_UTF8(*Setup.MapSize))));
		const sov::MapSizeType& T = Rules->mapSizes[static_cast<size_t>((At + D + N) % N)];
		Setup.MapSize = Str(T.id);
		Setup.Players = FMath::Clamp(T.defaultPlayers, 2, static_cast<int32>(Rules->civs.size()));
	}));
	Add(Option(TEXT("Civilizations"), [this]() { return FString::FromInt(Setup.Players); },
		[this](int32 D) { Setup.Players = FMath::Clamp(Setup.Players + D, 2, static_cast<int32>(Rules->civs.size())); }));
	Add(Option(TEXT("Difficulty"), [this]() { return FString(kLevels[FMath::Clamp(Setup.Difficulty, 0, 7)]); },
		[this](int32 D) { Setup.Difficulty = FMath::Clamp(Setup.Difficulty + D, 0, 7); }));
	Add(Option(TEXT("Length"), [this]() { return FString(kSpeeds[IndexOf(kSpeeds, Setup.Speed)].Label); },
		[this](int32 D) { Setup.Speed = kSpeeds[(IndexOf(kSpeeds, Setup.Speed) + D + UE_ARRAY_COUNT(kSpeeds)) % UE_ARRAY_COUNT(kSpeeds)].Id; }));
	Add(Option(TEXT("Begin in"), [this]() { return FString(kEras[IndexOf(kEras, Setup.StartEra)].Label) + TEXT(" era"); },
		[this](int32 D) { Setup.StartEra = kEras[(IndexOf(kEras, Setup.StartEra) + D + UE_ARRAY_COUNT(kEras)) % UE_ARRAY_COUNT(kEras)].Id; }));
	Add(Option(TEXT("Natural disasters"), [this]() { return FString(kDisasters[FMath::Clamp(Setup.Disasters, -1, 4) + 1]); },
		[this](int32 D) { Setup.Disasters = FMath::Clamp(Setup.Disasters + D, -1, 4); }));
	Add(Option(TEXT("Barbarian Clans"), [this]() { return FString(Setup.bClans ? TEXT("On") : TEXT("Off")); }, [this](int32) { Setup.bClans = !Setup.bClans; }));
	Add(Option(TEXT("Monopolies and Corporations"), [this]() { return FString(Setup.bMonopolies ? TEXT("On") : TEXT("Off")); },
		[this](int32) { Setup.bMonopolies = !Setup.bMonopolies; }));
	Add(Option(TEXT("Rivals remember you"), [this]() { return FString(Setup.bRivalMemory ? TEXT("On") : TEXT("Off")); },
		[this](int32) { Setup.bRivalMemory = !Setup.bRivalMemory; }));
	Add(Option(TEXT("Map seed"), [this]() { return FString::Printf(TEXT("%llu"), Setup.Seed); },
		[this](int32) { Setup.Seed = FPlatformTime::Cycles64() % 999999 + 1; }));

	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f, 0.014f, 0.013f, 0.97f)).Padding(0)
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1240.f).HeightOverride(780.f)
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(18, 14))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[SNew(STextBlock).Font(FSovStyle::Font(24, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(TEXT("New game")))]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SHorizontalBox)
						// Civs.
						+ SHorizontalBox::Slot().AutoWidth()
						[SNew(SBox).WidthOverride(280.f)[SNew(SScrollBox) + SScrollBox::Slot()[Civs]]]
						// The chosen civ.
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(16, 0)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[SNew(STextBlock).Font(FSovStyle::Font(22, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() {
									return FText::FromString(CivPick < 0 ? FString(TEXT("Random civilization")) : Str(Rules->civs[static_cast<size_t>(CivPick)].name));
								})]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
								[SNew(STextBlock).Font(FSovStyle::Font(13)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() {
									return FText::FromString(CivPick < 0 ? FString(TEXT("Any of the twelve; you learn which when the game begins."))
																		 : LeaderName(*Rules, CivPick) + TEXT(", ") + Str(Rules->civs[static_cast<size_t>(CivPick)].leaning));
								})]
								+ SVerticalBox::Slot().AutoHeight()
								[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(CivText(CivPick)); })]
							]
						]
						// Options.
						+ SHorizontalBox::Slot().AutoWidth()
						[SNew(SBox).WidthOverride(400.f)[SNew(SScrollBox) + SScrollBox::Slot()[Options]]]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ContentPadding(FMargin(18, 6))
							.OnClicked_Lambda([this]() { OnBack.ExecuteIfBound(); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(13, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Back")))]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Primary()).ContentPadding(FMargin(28, 8))
							.OnClicked_Lambda([this]() { Start(); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(15, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Start the game")))]
						]
					]
				]
			]
		]
	];
}

void SSovSetupScreen::Start()
{
	FSovSetup Out = Setup;
	const int32 N = static_cast<int32>(Rules->civs.size());
	const int32 Pick = CivPick >= 0 ? CivPick : (N > 0 ? static_cast<int32>(FPlatformTime::Cycles64() % N) : -1);
	Out.Civ = Pick >= 0 ? Str(Rules->civs[static_cast<size_t>(Pick)].id) : FString();
	OnStart.ExecuteIfBound(Out);
}

FReply SSovSetupScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return HandleKey(Event.GetKey()) ? FReply::Handled() : SCompoundWidget::OnKeyDown(Geometry, Event);
}

bool SSovSetupScreen::HandleKey(const FKey& Key)
{
	// Up and Down pick a civ, Enter starts, Esc goes back.
	const int32 N = static_cast<int32>(Rules->civs.size());
	if (Key == EKeys::Up) CivPick = FMath::Max(-1, CivPick - 1);
	else if (Key == EKeys::Down) CivPick = FMath::Min(N - 1, CivPick + 1);
	else if (Key == EKeys::Enter) Start();
	else if (Key == EKeys::Escape) OnBack.ExecuteIfBound();
	else return false;
	return true;
}

TSharedRef<SWidget> SSovSetupScreen::Option(const FString& Label, TFunction<FString()> Value, TFunction<void(int32)> Step)
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

FString SSovSetupScreen::CivText(int32 Civ) const
{
	if (Civ < 0)
	{
		return TEXT("Each civilization has its own ability, a unique unit and a unique building or improvement; its leader adds a second ability and an "
					"agenda that shapes how the AI treats you. Pick one on the left to read them.");
	}
	const sov::CivType& C = Rules->civs[static_cast<size_t>(Civ)];
	FString T;
	T += FString::Printf(TEXT("Civilization ability: %s\n%s\n\n"), *Str(C.ability.name), *Str(C.ability.text));
	T += FString::Printf(TEXT("Leader ability: %s\n%s\n\n"), *Str(C.leaderAbility.name), *Str(C.leaderAbility.text));
	if (!C.uniquesText.empty()) T += FString::Printf(TEXT("Uniques\n%s\n\n"), *Str(C.uniquesText).Replace(TEXT(". "), TEXT(".\n")));
	if (!C.agendaName.empty()) T += FString::Printf(TEXT("Agenda: %s\n%s\n\n"), *Str(C.agendaName), *Str(C.agendaText));
	if (const sov::Dynasty* D = Rules->dynastyOf(static_cast<sov::TypeIndex>(Civ)); D && D->names.size() > 1)
	{
		FString Heirs;
		for (size_t i = 1; i < D->names.size(); ++i) Heirs += (Heirs.IsEmpty() ? TEXT("") : TEXT(", ")) + Str(D->names[i]);
		T += TEXT("Heirs: ") + Heirs;
	}
	return T;
}
