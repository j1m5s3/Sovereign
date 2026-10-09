# Record: ransoming a captured leader (leader doc §5)

Status: done, 2026-10-09. Previous: `2026-10-09-leader-loyalty.md`. The leader doc makes a captured leader a deal-screen item, ransomed for gold, cities or peace; until now the only way out was abandoning the captive, which the AI did at once. Chosen by Claude under James's standing consent; the values are Sovereign tuning.

## Rules

- **The Ruler deal item** (`DealItemKind::Ruler`, its amount the captive's own civ): only the captor gives it, only to that civ, once per deal (`Game::dealProblem`). At war the deal needs peace in it, as every deal does. Accepted, the ruler comes home to the capital (else any city, else the start plot) with its saved loadout and promotions, under the same name, and the interregnum ends at the start of that civ's next turn (`Game::ransomRuler`). A ransom costs no loyalty or era score; abandoning still does (`2026-10-09-leader-loyalty.md`).
- **Its value** (`Game::dealValue`): to the captive's civ 150 + 30 per city (up to 10) + 75 per level; to the captor −(50 + 25 per level).
- **The AI** (`ai::deals`, at peace): it offers 100 + 50 per level Gold for its own ruler, then twice and three times that, and proposes the cheapest the captor would take. An AI captor offers a human's ruler back for twice that price. It gives the captive up after `kRansomPatience` (10) turns without a ransom, or at once when the captor is no major civ (Free Cities, barbarians), then crowns a successor as before.
- **Talk** (diplomacy/): "ruler" is a deal kind the model and the scripted reader can name; whichever side holds the other's captive is read as its giver.
- **Saves:** `Player::capturedTurn` (save version 86).

## Results

- 128 AI games (Small, 6 AI, turn 200) against #244: no measurable change (inferred: captures, and so ransoms, are rare before turn 200).
- 8 long AI games in 8 setups (up to Huge, 400 turns) end with no crash or replay mismatch; 5 setups saved and reloaded at 8 cuts end on the straight game's state.
- The 30-turn golden game's state hash changes, as every save does with the new field (inferred: no leader is captured in it); the rules checksum is unchanged.

## Tests

- `a_captured_ruler_is_ransomed_home` (saved while held; only the captor offers it, to its civ, once; the text; home with its loadout, promotions and name; the interregnum ending; too cheap for the captor), `the_ai_ransoms_its_ruler_or_gives_it_up` (the cheapest price taken; waiting without gold; giving up after 10 turns), `the_ai_gives_up_a_ruler_no_one_will_ransom`, `the_ai_offers_a_human_its_ruler_back`, `a_captured_leader_holds_the_throne_until_abandoned` (the capture turn), and in diplomacy/ `a_captured_ruler_is_asked_for_in_words`.
- Mutation check: 22 mutants (each check, value, effect, AI rule and dialogue rule broken in turn); the tests catch all 22 (the too-cheap offer, the capture turn, the barbarian captor and the second captive direction in talk were added to catch the last four).

## Left open

- **Unreal:** nothing new is needed as far as the code shows: an AI's ransom offer reaches a human through the existing offer line (`SovHUD.cpp`, `describeDeal`), and a human proposes deals by talking, where "ruler" is now understood. Not tried in the editor.
- An eliminated captor's prisoners stay held; their civ can still abandon them.
