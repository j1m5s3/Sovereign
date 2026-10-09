// The new-game setup screen (plan D, step 5): pick a civ and leader, seeing their abilities, uniques and
// agenda, then the map and game options, and start.
#pragma once

#include "CoreMinimal.h"
#include "SovSession.h"
#include "Widgets/SCompoundWidget.h"

namespace sov
{
class Rules;
}

class SSovSetupScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovSetupScreen) {}
	SLATE_ARGUMENT(TSharedPtr<const sov::Rules>, Rules)  // the civs and options to offer
	SLATE_ARGUMENT(FSovSetup, Setup)                     // where the choices start
	SLATE_EVENT(TDelegate<void(const FSovSetup&)>, OnStart)
	SLATE_EVENT(TDelegate<void()>, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	// Up, Down, Enter or Esc (from the widget, or forwarded by the controller when the viewport has focus).
	bool HandleKey(const FKey& Key);

private:
	TSharedRef<SWidget> Option(const FString& Label, TFunction<FString()> Value, TFunction<void(int32)> Step);
	FString CivText(int32 Civ) const;
	void Start();

	TSharedPtr<const sov::Rules> Rules;
	FSovSetup Setup;
	int32 CivPick = -1;  // index into Rules->civs (-1: random)
	TDelegate<void(const FSovSetup&)> OnStart;
	TDelegate<void()> OnBack;
};
