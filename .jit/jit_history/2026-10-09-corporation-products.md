# Record: Corporation Products (Monopolies and Corporations mode)

Status: done, 2026-10-09. Previous: `2026-10-06-monopolies.md`, which built Industries, Corporations and Monopolies and left Products out. Spec: 07-economy-trade-great-people (Monopolies & Corporations, an outline only: Corporations "produce Products (Great Works) for tourism"). The values are Sovereign's own.

## Built

- **Product** is a Great Work type (`PRODUCT` in `greatpeople.json`): +2 Gold, 4 Tourism, slots of type PRODUCT. While the mode is on, a Stock Exchange or Seaport holds three of those slots (`Game::extraProductSlots`). A Palace slot does not take one.
- **CreateProduct** (save version 91): a Great Merchant on the land of a city that has a Corporation (and a free Product slot) makes one Product into that slot and is spent. Each Corporation makes three (`Plot::products`). Not their unique activation; they cannot do both.
- **Effects:** the work's Gold and Tourism, and +10% Gold in the city that holds it, stacked with the Industry/Corporation percent there. Products move and trade as other Great Works.
- **AI:** merchants in the mode make a Product where they stand if they can, and walk to a Corporation's city with a free slot before their own district (`greatPerson` in `ai.cpp`).

## Tests

- `a_great_merchant_makes_a_product_in_a_corporation_city`: Zhang Qian in a Corporation's city with a Stock Exchange; tourism +4, Gold up, the merchant gone, the save keeps the work.
- `products_need_a_corporation_and_their_own_slot`: a Palace is not enough; an Industry is not enough; the mode off grants no slots.
- `a_corporation_makes_three_products`: a fourth is refused when slots remain.
- `ai_merchants_make_products`: the AI spends the merchant on a Product.

## Left open

- Each luxury's own Industry effect from Civ VI (still not built; the mode's Gold percents stay as in `2026-10-06-monopolies.md`).
- Unreal: the unit panel has no Create Product control (a PC session task; this work does not edit Unreal).
