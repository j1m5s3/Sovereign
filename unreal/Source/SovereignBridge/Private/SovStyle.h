// The look of every Slate screen (plan D): palette, fonts, panel and button brushes, and the icon set
// drawn by tools/ui/icons.py into Content/Slate/Icons (PNG, loaded at run time).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

class FSlateStyleSet;
struct FSlateBrush;

class FSovStyle
{
public:
	static const FSlateStyleSet& Get();

	// An icon by name ("food", "endturn"...); a blank brush when the file is missing.
	static const FSlateBrush* Icon(FName Name);
	static FSlateFontInfo Font(int32 Size, bool bBold = false);

	static const FSlateBrush* Panel();      // dark parchment-edged panel
	static const FSlateBrush* Bar();        // the top bar strip
	static const FSlateBrush* Backdrop();
	// A unit's portrait (Content/Slate/Portraits/<Figure>.png, tools/art/blender/unit_portraits.py): a culture's dress
	// (Archer_Asian) falls back to the plain figure (Archer); null when there is none.
	static const FSlateBrush* Portrait(FName Figure);   // the main menu's painted scene (tools/art/blender/backdrop.py)
	static const FButtonStyle& Button();    // plain button
	static const FButtonStyle& Primary();   // the gold call-to-action (end turn)
	static const FProgressBarStyle& Progress();  // plain bars the fill colour shows on

	// Palette.
	static const FLinearColor Ink;      // panel fill
	static const FLinearColor Edge;     // panel outline (bronze)
	static const FLinearColor Text;     // parchment text
	static const FLinearColor Dim;      // secondary text
	static const FLinearColor Gold;     // highlights
	static const FLinearColor Good;     // positive numbers
	static const FLinearColor Bad;      // negative numbers, warnings
};
