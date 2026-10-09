# Record: every AI weighs its religion's beliefs (step 6, "everything else")

Status: done, 2026-10-08. Chosen by Claude under James's standing consent, found while trying the AI's policy cards on a copy of the game.

## What was wrong

- **Only player 0's AI weighed its religion's beliefs.** `religionScore` (ai.cpp) founds the religion on a copy of the game and scores our cities' reports there. The copy came from `Game::fromScenario`, which begins player 0's turn, so any other player's founding was refused on it (NotYourTurn). Every candidate then scored alike and the AI kept the first Founder and Follower beliefs listed: Cross-Cultural Dialogue and Choral Music, or Lay Ministry and Divine Inspiration once those were taken. In 8 AI games to turn 200, 12 of the 14 religions were founded by other players, every one with those pairs.

## Fix

- The copy is the game as it stands, on the AI's own turn (the `Game` constructor that loading a save uses), so the founding goes through for any player. The pantheon's copies set the pantheon on the state and found nothing, so they were right already.

## Results

The same 8 games: the other players' religions now take Tithe (5), Work Ethic (5), Sacred Places (4), Divine Inspiration (3), Feed the World (2) and others.

64 seeds (Small, 6 AI, turn 200):

| Build | cities | pop | techs | civics | science | culture | prod | gold |
|---|---|---|---|---|---|---|---|---|
| main | 8.2 | 49.2 | 29.1 | 20.8 | 71.8 | 50.2 | 117.7 | 170 |
| this | 8.2 | 49.2 | 28.9 | 20.7 | 72.7 | 49.9 | 118.5 | 175 |

Religion is a small part of an AI's economy by turn 200: the change is within the noise there.

Save and reload: 8 games in 5 setups, each saved and loaded once or twice, end on the straight game's state. The four replay games of the speed-up checks replay their own command logs.

## Tests

- `ai_weighs_religion_beliefs_on_its_own_turn`: two players of one civ, each with a capital, a Holy Site and a Shrine. Whichever has the Great Prophet founds with the same beliefs, Tithe and Feed the World, not the first ones listed. Before the fix player 1 took Cross-Cultural Dialogue and Choral Music.
