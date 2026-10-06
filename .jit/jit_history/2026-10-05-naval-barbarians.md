# Plan: naval barbarians (step 6, "everything else")

Status: done, 2026-10-05. Previous: `jit_history/2026-10-05-encampment.md`. Chosen by Claude under James's standing consent: coastal camps of the naval tribe, left out of the rules core because "naval tribes wait for naval movement", which has long existed. One milestone, one PR.

Specs: 01-map-and-terrain (Barbarians: "Coastal camps are naval tribes"; "Naval camps spawn Galleys/Quadriremes and raid coasts"); data/barbarians-goody-huts (Barbarian tribes).

## Milestone

1. **Done:** a camp placed next to shallow water belongs to the naval tribe (TRIBE_NAVAL); it releases the strongest naval melee unit (ranged: naval ranged) that half the majors can build, onto a free neighbouring shallow-water plot. Its ships then act like any barbarian unit (the movement and attack code is domain-agnostic): they head for war targets and coastal cities once bold enough and attack when the odds look good.

## Decisions (Claude's recommendations; James gave standing consent)

- Their ships raid the coast beside them (coastal raids, recorded in `jit_history/2026-10-05-pillage.md`). The scout that turns a camp aggressive is still not modelled.
- A 6-AI Deity test game showed naval barbarians at sea (up to 6 at a time) and ran to its end at turn 359.
