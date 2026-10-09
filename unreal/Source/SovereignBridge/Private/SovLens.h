// Map lenses (plan D, step 6): recolour the revealed plots to show one thing (religion, loyalty, appeal,
// where a city may be founded, trade routes), with a legend for the screen.
#pragma once

#include "CoreMinimal.h"

struct FSovMirror;
namespace sov
{
class Game;
}

enum class ESovLens : uint8
{
	None,
	Religion,
	Loyalty,
	Appeal,
	Settler,
	Trade,
	Count
};

struct FSovLensKey
{
	FLinearColor Color;
	FString Label;
};

const TCHAR* SovLensName(ESovLens Lens);
FName SovLensIcon(ESovLens Lens);
// Tints the mirror's plots for the lens (None leaves them) and lists what the colours mean.
void SovApplyLens(FSovMirror& Mirror, const sov::Game& Game, int32 Viewer, ESovLens Lens, TArray<FSovLensKey>* Legend = nullptr);
