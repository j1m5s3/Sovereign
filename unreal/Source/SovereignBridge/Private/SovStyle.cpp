#include "SovStyle.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"

const FLinearColor FSovStyle::Ink(0.018f, 0.016f, 0.014f, 0.98f);
const FLinearColor FSovStyle::Edge(0.42f, 0.28f, 0.11f, 1.f);
const FLinearColor FSovStyle::Text(0.93f, 0.89f, 0.80f, 1.f);
const FLinearColor FSovStyle::Dim(0.62f, 0.58f, 0.50f, 1.f);
const FLinearColor FSovStyle::Gold(1.f, 0.78f, 0.32f, 1.f);
const FLinearColor FSovStyle::Good(0.55f, 0.85f, 0.45f, 1.f);
const FLinearColor FSovStyle::Bad(0.95f, 0.42f, 0.36f, 1.f);

namespace
{
TSharedPtr<FSlateStyleSet> GStyle;
FButtonStyle GButton, GPrimary;
FProgressBarStyle GProgress;

const char* const kIcons[] = {"food", "production", "gold", "science", "culture", "faith", "favor", "tourism", "housing", "amenity", "strength",
	"ranged", "moves", "health", "experience", "skip", "fortify", "sleep", "found", "build", "promote", "trade", "religion", "greatperson", "gear",
	"link", "streets", "attack", "endturn", "menu", "research", "civic", "government", "era"};

FButtonStyle MakeButton(const FLinearColor& Fill, const FLinearColor& Hover, const FLinearColor& Down, const FLinearColor& Outline)
{
	FButtonStyle B = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	B.SetNormal(FSlateRoundedBoxBrush(Fill, 4.f, Outline, 1.f));
	B.SetHovered(FSlateRoundedBoxBrush(Hover, 4.f, FSovStyle::Gold, 1.f));
	B.SetPressed(FSlateRoundedBoxBrush(Down, 4.f, FSovStyle::Gold, 1.f));
	B.SetDisabled(FSlateRoundedBoxBrush(Fill * 0.6f, 4.f, Outline * 0.5f, 1.f));
	B.SetNormalPadding(FMargin(6.f, 4.f));
	B.SetPressedPadding(FMargin(6.f, 5.f, 6.f, 3.f));
	return B;
}

void Build()
{
	GStyle = MakeShared<FSlateStyleSet>("SovStyle");
	GStyle->SetContentRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Slate")));  // Slate loads image files by full path
	for (const char* Name : kIcons)
	{
		const FString File = GStyle->RootToContentDir(FString(TEXT("Icons/")) + UTF8_TO_TCHAR(Name), TEXT(".png"));
		// Dynamic brushes load their file on first use (static ones load only for styles registered at start-up).
		GStyle->Set(FName(FString(TEXT("Icon.")) + UTF8_TO_TCHAR(Name)), new FSlateDynamicImageBrush(FName(*File), FVector2D(24.f, 24.f)));
	}
	GStyle->Set("Panel", new FSlateRoundedBoxBrush(FSovStyle::Ink, 6.f, FSovStyle::Edge, 1.5f));
	GStyle->Set("Bar", new FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.018f, 0.016f, 0.9f), 0.f, FSovStyle::Edge, 1.f));
	GButton = MakeButton(FLinearColor(0.09f, 0.075f, 0.06f, 0.95f), FLinearColor(0.16f, 0.12f, 0.08f, 1.f), FLinearColor(0.22f, 0.16f, 0.09f, 1.f), FSovStyle::Edge);
	GProgress = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	GProgress.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 2.f));
	GProgress.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.f));
	GProgress.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.f));
	GPrimary = MakeButton(FLinearColor(0.42f, 0.27f, 0.07f, 1.f), FLinearColor(0.56f, 0.37f, 0.1f, 1.f), FLinearColor(0.66f, 0.45f, 0.12f, 1.f), FSovStyle::Gold);
}
}  // namespace

const FSlateStyleSet& FSovStyle::Get()
{
	if (!GStyle.IsValid()) Build();
	return *GStyle;
}

const FSlateBrush* FSovStyle::Icon(FName Name)
{
	return Get().GetBrush(FName(FString(TEXT("Icon.")) + Name.ToString()));
}

FSlateFontInfo FSovStyle::Font(int32 Size, bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

const FSlateBrush* FSovStyle::Panel() { return Get().GetBrush("Panel"); }
const FSlateBrush* FSovStyle::Bar() { return Get().GetBrush("Bar"); }

const FButtonStyle& FSovStyle::Button()
{
	Get();
	return GButton;
}

const FProgressBarStyle& FSovStyle::Progress()
{
	Get();
	return GProgress;
}

const FButtonStyle& FSovStyle::Primary()
{
	Get();
	return GPrimary;
}
