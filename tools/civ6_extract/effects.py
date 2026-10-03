"""Render Civ VI Modifiers / RequirementSets as short plain-language text.

Each modifier is (ModifierType -> CollectionType + EffectType, arguments,
owner requirement set, subject requirement set). We render:

    <effect phrase> [to <collection>] [where <subject reqs>] [(while <owner reqs>)]

Templates cover the effect and requirement types that occur most often in the
shipped data; anything else falls back to a humanised form of the effect type
plus its arguments, so no information is dropped.
"""
from __future__ import annotations

import re
import sqlite3

from names import Names, humanize

# ---------------------------------------------------------------- templates --
# Placeholders: {Arg} value (names resolved), {+Arg} signed number,
# {Arg%} signed percent, {?Arg:text} text only when Arg is present and truthy.

EFFECTS = {
    "EFFECT_ADJUST_UNIT_TAG_ERA_PRODUCTION": "{Amount%} Production toward {UnitPromotionClass} units{?EraType: of {EraType}}",
    "EFFECT_ADJUST_BUILDING_YIELD_CHANGE": "{+Amount} {YieldType} from {BuildingType}",
    "EFFECT_ADJUST_PLOT_YIELD": "{+Amount} {YieldType}",
    "EFFECT_ADJUST_PLAYER_STRENGTH_MODIFIER": "{+Amount} Combat Strength",
    "EFFECT_ADJUST_UNIT_COMBAT_STRENGTH": "{+Amount} Combat Strength",
    "EFFECT_ADJUST_ATTACKER_STRENGTH_MODIFIER": "{+Amount} Combat Strength when attacking",
    "EFFECT_ADJUST_DEFENDER_STRENGTH_MODIFIER": "{+Amount} Combat Strength when defending",
    "EFFECT_TERRAIN_ADJACENCY": "{DistrictType} gets {+Amount} {YieldType} per {TilesRequired} adjacent {TerrainType}",
    "EFFECT_FEATURE_ADJACENCY": "{DistrictType} gets {+Amount} {YieldType} per adjacent {FeatureType}",
    "EFFECT_IMPROVEMENT_ADJACENCY": "{DistrictType} gets {+Amount} {YieldType} per {TilesRequired} adjacent {ImprovementType}",
    "EFFECT_DISTRICT_ADJACENCY": "{DistrictType} gets {+Amount} {YieldType} adjacency per adjacent district",
    "EFFECT_RIVER_ADJACENCY": "{DistrictType} gets {+Amount} {YieldType} adjacency next to a river",
    "EFFECT_ADJUST_GREAT_PERSON_POINTS": "{+Amount} {GreatPersonClassType} points per turn",
    "EFFECT_ADJUST_GREAT_PERSON_POINTS_PERCENT": "{Amount%} {GreatPersonClassType} points",
    "EFFECT_ADJUST_DISTRICT_GREAT_PERSON_POINTS": "{+Amount} {GreatPersonClassType} points from the district",
    "EFFECT_ADJUST_CITY_YIELD_MODIFIER": "{Amount%} {YieldType}",
    "EFFECT_ADJUST_CITY_YIELD_CHANGE": "{+Amount} {YieldType}",
    "EFFECT_ADJUST_CITY_YIELD_PER_POPULATION": "{+Amount} {YieldType} per Citizen",
    "EFFECT_ADJUST_CITY_YIELD_PER_DISTRICT": "{+Amount} {YieldType} per district",
    "EFFECT_ADJUST_CITY_ALL_YIELDS_CHANGE": "{+Amount} to all yields",
    "EFFECT_ADJUST_PLAYER_YIELD_CHANGE": "{+Amount} {YieldType} per turn",
    "EFFECT_ADJUST_DISTRICT_YIELD_MODIFIER": "{Amount%} to the district's {YieldType} (adjacency) yield",
    "EFFECT_ADJUST_DISTRICT_YIELD_CHANGE": "{+Amount} {YieldType} from the district",
    "EFFECT_ADJUST_DISTRICT_BASE_YIELD_CHANGE": "{+Amount} base {YieldType} from the district",
    "EFFECT_ADJUST_BUILDING_YIELD_MODIFIERS_FOR_DISTRICT": "{Amount%} {YieldType} from buildings in {DistrictType}",
    "EFFECT_ADJUST_BUILDING_YIELD_MODIFIER": "{Amount%} {YieldType} from {BuildingType}",
    "EFFECT_ADJUST_BUILDING_FEATURE_YIELD_CHANGE": "{+Amount} {YieldType} per adjacent {FeatureType}",
    "EFFECT_ADD_RELIGIOUS_BELIEF_YIELD": "{+Amount} {YieldType} per {PerXItems} {BeliefYieldType}{?DistrictType: ({DistrictType})}",
    "EFFECT_ADD_PLAYER_BELIEF_YIELD": "{+Amount} {YieldType} per {PerXItems} {BeliefYieldType}",
    "EFFECT_ADJUST_CITY_GREATWORK_YIELD": "Great Works of type {GreatWorkObjectType} give {+YieldChange} {YieldType}{?ScalingFactor: (scaling {ScalingFactor}%)}",
    "EFFECT_GRANT_UNIT_IN_CITY": "grants {Amount} {UnitType}",
    "EFFECT_GRANT_UNIT_WITH_EXPERIENCE": "grants a {UnitType}{?Experience: with {Experience} XP}",
    "EFFECT_GRANT_UNIT_IN_EACH_DISTRICT": "grants a {UnitType} in each district",
    "EFFECT_ADJUST_UNIT_SPY_OPERATION_CHANCE": "{+Amount} spy levels for {OperationType}",
    "EFFECT_GRANT_YIELD": "one-time grant of {Amount} {YieldType}",
    "EFFECT_GRANT_RANDOM_TECHNOLOGY_BOOST_BY_ERA": "grants {Amount} random Eureka(s) ({StartEraType} to {EndEraType})",
    "EFFECT_GRANT_RANDOM_CIVIC_BOOST_BY_ERA": "grants {Amount} random Inspiration(s) ({StartEraType} to {EndEraType})",
    "EFFECT_GRANT_ALL_TECHNOLOGY_BOOST_BY_ERA": "grants all Eurekas from {StartEraType} to {EndEraType}",
    "EFFECT_GRANT_RANDOM_TECHNOLOGY_BOOST_ON_NEW_ERA": "{Amount} random Eureka(s) on each new era",
    "EFFECT_GRANT_RANDOM_CIVIC_BOOST_ON_NEW_ERA": "{Amount} random Inspiration(s) on each new era",
    "EFFECT_GRANT_PLAYER_RANDOM_TECHNOLOGY": "grants {Amount} random technology",
    "EFFECT_GRANT_PLAYER_RANDOM_CIVIC": "grants {Amount} random civic",
    "EFFECT_GRANT_PLAYER_SPECIFIC_TECHNOLOGY": "grants the technology {TechType}",
    "EFFECT_GRANT_PLAYER_SPECIFIC_TECH_BOOST_GREAT_PERSON": "Eureka for {TechType}{?GrantTechIfBoosted: (or the whole tech if already boosted)}",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_MODIFIER": "{Amount%} {YieldType} from trade routes",
    "EFFECT_ADJUST_CITY_TOURISM": "Tourism from {?GreatWorkObjectType:{GreatWorkObjectType} }{?ImprovementType:{ImprovementType} }scaled {ScalingFactor}%",
    "EFFECT_ADJUST_UNIT_MOVEMENT": "{+Amount} Movement",
    "EFFECT_ADJUST_UNIT_SEA_MOVEMENT": "{+Amount} Movement for naval units",
    "EFFECT_ADJUST_PLAYER_EMBARKED_UNIT_MOVEMENT": "{+Amount} Movement while embarked",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_FOR_INTERNATIONAL": "international trade routes {+Amount} {YieldType}",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_FOR_DOMESTIC": "domestic trade routes {+Amount} {YieldType}",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_TO_OTHERS": "trade routes from other civs give them {+Amount} {YieldType}",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_FROM_OTHERS": "{+Amount} {YieldType} for each trade route other civs send here",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_CHANGE": "trade routes {+Amount} {YieldType}",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD": "trade routes {+Amount} {YieldType}",
    "EFFECT_ADJUST_CITY_TRADE_ROUTE_YIELD_FOR_INTERNATIONAL": "international routes from the city {+Amount} {YieldType}",
    "EFFECT_ADJUST_CITY_TRADE_ROUTE_YIELD_FOR_DOMESTIC": "domestic routes from the city {+Amount} {YieldType}",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_PER_SPECIALTY_DISTRICT_FOR_DOMESTIC": "domestic routes {+Amount} {YieldType} per specialty district at destination",
    "EFFECT_ADJUST_TRADE_ROUTE_YIELD_PER_SPECIALTY_DISTRICT_FOR_INTERNATIONAL": "international routes {+Amount} {YieldType} per specialty district at destination",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_IMPROVEMENT_IN_TARGET_CITY": "trade routes {+Amount} {YieldType} per {ImprovementType} in the destination",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_TERRAIN_FOR_DOMESTIC": "domestic routes {+Amount} {YieldType} per {TerrainType} in the origin",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_TERRAIN_FOR_INTERNATIONAL": "international routes {+Amount} {YieldType} per {TerrainType} in the origin",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_ORIGIN_YIELD_FOR_ALLY_ROUTE": "routes to allies {+Amount} {YieldType} to origin",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_DESTINATION_YIELD_FOR_ALLY_ROUTE": "routes to allies {+Amount} {YieldType} to destination",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_ORIGIN_YIELD_FOR_SUZERAIN_ROUTE": "routes to city-states you are suzerain of {+Amount} {YieldType} to origin",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_DESTINATION_YIELD_FOR_SUZERAIN_ROUTE": "routes to city-states you are suzerain of {+Amount} {YieldType} to destination",
    "EFFECT_ADJUST_CITY_STATE_TRADE_ROUTE_DISTRICT_YIELD": "routes to city-states {+Amount} {YieldType} per district",
    "EFFECT_ADJUST_CITY_STATE_TRADE_ROUTE_FLAT_YIELD": "routes to city-states {+Amount} {YieldType}",
    "EFFECT_ADJUST_CITY_TRADE_ROUTE_YIELD_PER_DESTINATION_LUXURY_RESOURCE_FOR_INTERNATIONAL": "international routes {+Amount} {YieldType} per luxury at destination",
    "EFFECT_ADJUST_CITY_TRADE_ROUTE_YIELD_PER_DESTINATION_STRATEGIC_RESOURCE_FOR_INTERNATIONAL": "international routes {+Amount} {YieldType} per strategic at destination",
    "EFFECT_ADJUST_CITY_TRADE_ROUTE_YIELD_PER_LOCAL_BONUS_RESOURCE_FOR_INTERNATIONAL": "international routes {+Amount} {YieldType} per bonus resource in origin",
    "EFFECT_ADJUST_TRADE_ROUTE_CAPACITY": "{+Amount} Trade Route capacity",
    "EFFECT_ADJUST_UNIT_HEALING_MODIFIERS": "{+Amount} HP healing ({Type})",
    "EFFECT_ADJUST_UNIT_EXPERIENCE_MODIFIER": "{Amount%} combat XP",
    "EFFECT_ADJUST_UNIT_GRANT_EXPERIENCE": "{?Amount:grants {Amount} XP}",
    "EFFECT_ADJUST_CITY_HAPPINESS_YIELD": "{Amount%} {YieldType} while {HappinessType}",
    "EFFECT_ADJUST_CITY_FREE_POWER": "{+Amount} Power ({SourceType})",
    "EFFECT_ADJUST_UNIT_ROCK_BAND_LEVEL_DISTRICT": "{+Amount} Rock Band level when performing at {DistrictType}",
    "EFFECT_ADJUST_CITY_IDENTITY_PER_TURN": "{+Amount} Loyalty per turn",
    "EFFECT_GRANT_YIELD_BASED_ON_CURRENT_YIELD_RATE": "grants {YieldToGrant} equal to {Multiplier}x current {YieldToBaseOn} per turn",
    "EFFECT_ADJUST_WONDER_ERA_PRODUCTION": "{Amount%} Production toward wonders ({StartEra} to {EndEra})",
    "EFFECT_ADJUST_DISTRICT_PRODUCTION": "{Amount%} Production toward {DistrictType}",
    "EFFECT_ADJUST_BUILDING_PRODUCTION": "{Amount%} Production toward {?BuildingType:{BuildingType}}{?DistrictType:buildings in {DistrictType}}",
    "EFFECT_ADJUST_UNIT_PRODUCTION": "{Amount%} Production toward {UnitType}",
    "EFFECT_ADJUST_UNIT_DOMAIN_PRODUCTION": "{Amount%} Production toward {Domain} units",
    "EFFECT_ADJUST_PROJECT_PRODUCTION": "{Amount%} Production toward {ProjectType}",
    "EFFECT_ADJUST_ALL_PROJECTS_PRODUCTION": "{Amount%} Production toward projects",
    "EFFECT_ADJUST_ALL_DISTRICTS_PRODUCTION": "{Amount%} Production toward districts",
    "EFFECT_ADJUST_CITY_PRODUCTION_UNIT": "{Amount%} Production toward units",
    "EFFECT_ADJUST_CITY_PRODUCTION_DISTRICT": "{Amount%} Production toward districts",
    "EFFECT_ADJUST_CITY_PRODUCTION_BUILDING": "{Amount%} Production toward buildings",
    "EFFECT_ADJUST_CITY_ALL_MILITARY_UNITS_PRODUCTION": "{Amount%} Production toward military units",
    "EFFECT_ADJUST_SPACE_RACE_PROJECTS_PRODUCTION": "{Amount%} Production toward space race projects",
    "EFFECT_ADJUST_CITY_CORPS_ARMY_PRODUCTION": "{Amount%} Production toward corps/armies",
    "EFFECT_ADJUST_UNIT_POST_COMBAT_YIELD": "after killing a unit gain {YieldType} = {PercentDefeatedStrength}% of its Combat Strength",
    "EFFECT_ADD_DIPLOMATIC_YIELD_MODIFIER": "{Amount%} {YieldType} ({DiplomaticYieldSource})",
    "EFFECT_ADJUST_TERRAIN_YIELD_FROM_ADJACENT_IMPROVEMENTS": "{TerrainType} tiles {+Amount} {YieldType} per adjacent {ImprovementType}",
    "EFFECT_ADJUST_PLAYER_DIPLOMATIC_VICTORY_POINTS": "{+Amount} Diplomatic Victory Points",
    "EFFECT_ADJUST_DISTRICT_TOURISM_CHANGE": "{+Amount} Tourism from the district",
    "EFFECT_ADJUST_WAR_WEARINESS": "{Amount%} war weariness",
    "EFFECT_ADJUST_PLAYER_FREE_RESOURCE_IMPORT": "{+Amount} {ResourceType} (import)",
    "EFFECT_ADJUST_PLAYER_FREE_RESOURCE_IMPORT_EXTRACTION": "{+Amount} {ResourceType} per turn",
    "EFFECT_GRANT_FREE_RESOURCE_IN_CITY": "{+Amount} {ResourceType} in the city",
    "EFFECT_GRANT_FREE_RESOURCE_EXTRACTED": "{+Amount} {ResourceType} per turn",
    "EFFECT_GRANT_PLAYER_FREE_RESOURCE_EXTRACTED": "{+Amount} {ResourceType} per turn",
    "EFFECT_ENABLE_UNIT_FAITH_PURCHASE": "can buy {Tag} units with Faith",
    "EFFECT_ENABLE_BUILDING_FAITH_PURCHASE": "can buy buildings in {DistrictType} with Faith",
    "EFFECT_ADJUST_GREAT_PERSON_GUARANTEE": "guarantees a {GreatPersonClassType}{?EraType: in {EraType}}",
    "EFFECT_ADJUST_ALL_UNITS_PURCHASE_COST": "{Amount}% cheaper to purchase {UnitDomain} units",
    "EFFECT_ADJUST_ALL_BUILDINGS_PURCHASE_COST": "{Amount}% cheaper to purchase buildings",
    "EFFECT_ADJUST_ALL_DISTRICTS_PURCHASE_COST": "{Amount}% cheaper to purchase districts",
    "EFFECT_ADJUST_UNIT_PURCHASE_COST": "{Amount}% cheaper to purchase {UnitType}",
    "EFFECT_ADJUST_BUILDING_PURCHASE_COST": "{Amount}% cheaper to purchase {BuildingType}",
    "EFFECT_GRANT_PRODUCTION_IN_CITY": "one-time {Amount} Production",
    "EFFECT_ADJUST_UNIT_IGNORE_TERRAIN_COST": "ignores {Type} movement costs",
    "EFFECT_ADJUST_PLAYER_GOVERNOR_POINTS": "{+Delta} Governor Title(s)",
    "EFFECT_ADJUST_DISTRICT_YIELD_BASED_ON_ADJACENCY_BONUS": "{YieldTypeToGrant} equal to the district's {YieldTypeToMirror} adjacency",
    "EFFECT_GRANT_UNIT_YIELD_ADJACENT_TERRAINS": "{+Amount} {YieldType} on adjacent {TerrainType}",
    "EFFECT_GRANT_UNIT_YIELD_ADJACENT_FEATURES": "{+Amount} {YieldType} on adjacent {FeatureType}",
    "EFFECT_ADJUST_EXTRA_GREAT_WORK_SLOTS": "{+Amount} {GreatWorkSlotType} slot(s) in {BuildingType}",
    "EFFECT_ADJUST_ADDITIONAL_PILLAGING": "{+Amount} extra {PlunderType} from pillaging {ImprovementType}",
    "EFFECT_ADJUST_UNIT_SPY_OPERATION_TIME": "{OperationType} {ReductionPercent}% faster",
    "EFFECT_ADJUST_DISTRICT_TOURISM_ADJACENCY_YIELD_MOFIFIER": "Tourism equal to {Amount}% of the district's {YieldType} adjacency",
    "EFFECT_GRANT_GREAT_PERSON_CLASS_IN_CITY": "grants {Amount} {GreatPersonClassType}",
    "EFFECT_GRANT_CITY_YIELD_PERCENT_BUILDING_CREATED_COST": "on completing a building, {YieldType} = {BuildingProductionPercent}% of its cost",
    "EFFECT_GRANT_CITY_YIELD_PERCENT_UNIT_CREATED_COST": "on training a unit, {YieldType} = {UnitProductionPercent}% of its cost",
    "EFFECT_ADJUST_UNIT_TOURISM_BOMB_DISTRICT": "{+Amount} Tourism burst at {DistrictType}",
    "EFFECT_ADJUST_PLAYER_RESOURCE_ACCUMULATION_MODIFIER": "{Amount%} {ResourceType} accumulation",
    "EFFECT_ADJUST_FEATURE_APPEAL_MODIFIER": "{FeatureType} {+Amount} Appeal",
    "EFFECT_ADJUST_CORPS_ARMY_PREREQ": "can form {?Corps:Corps}{?Domain: ({Domain})} with {CivicType}",
    "EFFECT_ADJUST_CORPS_ARMY_MODIFIED_STRENGTH": "corps/armies {+Amount} Combat Strength",
    "EFFECT_ADD_PLAYER_FAVOR": "{+Amount} Diplomatic Favor",
    "EFFECT_ADJUST_PLAYER_EXTRA_FAVOR_PER_TURN": "{+Amount} Diplomatic Favor per turn",
    "EFFECT_ADD_CULTURE_BOMB_TRIGGER": "culture bomb when {?DistrictType:{DistrictType}}{?ImprovementType:{ImprovementType}} is built",
    "EFFECT_GRANT_BUILDING_IN_CITY_IGNORE": "grants {BuildingType}",
    "EFFECT_ADJUST_POLICY_HOUSING": "{+Amount} Housing",
    "EFFECT_ADJUST_POLICY_AMENITY": "{+Amount} Amenity",
    "EFFECT_ADJUST_BUILDING_HOUSING": "{+Amount} Housing",
    "EFFECT_ADJUST_DISTRICT_HOUSING": "{+Amount} Housing",
    "EFFECT_ADJUST_IMPROVEMENT_HOUSING": "{+Amount} Housing",
    "EFFECT_ADJUST_DISTRICT_AMENITY": "{+Amount} Amenity",
    "EFFECT_ADJUST_IMPROVEMENT_AMENITY": "{+Amount} Amenity",
    "EFFECT_ADJUST_CITY_GROWTH": "{Amount%} growth",
    "EFFECT_ADJUST_CITY_APPEAL": "{+Amount} Appeal",
    "EFFECT_ADJUST_PLAYER_TERRAIN_WORK_IMPASSABLE_MODIFIER": "citizens can work {TerrainType}",
    "EFFECT_ADJUST_CITY_YIELD_FROM_POWERED_BUILDING": "{+Amount} {YieldType} per powered building",
    "EFFECT_ADD_PLAYER_PROJECT_AVAILABILITY": "unlocks project {ProjectType}",
    "EFFECT_ADJUST_PLAYER_VALID_IMPROVEMENT": "can build {ImprovementType}",
    "EFFECT_ADJUST_PLAYER_SPY_BONUS": "{+Amount} spy level{?Offense: (offense)}",
    "EFFECT_ADD_RELIGIOUS_BUILDING": "unlocks worship building {BuildingType}",
    "EFFECT_ADD_RELIGIOUS_BUILDING_MULTIPLIER": "+{Multiplier}% {YieldType} in cities with a worship building",
    "EFFECT_ADD_DIPLO_VISIBILITY": "{+Amount} diplomatic visibility",
    "EFFECT_GRANT_INFLUENCE_TOKEN": "grants {Amount} Envoy(s)",
    "EFFECT_ADJUST_INFLUENCE_POINTS_PER_TURN": "{+Amount} Influence points per turn",
    "EFFECT_ADJUST_UNIT_VALID_TERRAIN": "can enter {TerrainType}",
    "EFFECT_ADJUST_RANDOM_EVENT_NO_UNIT_DAMAGE": "no unit damage from {RandomEventType}",
    "EFFECT_ADJUST_PLOT_PURCHASE_COST_TERRAIN": "{Amount%} tile purchase cost on {TerrainType}",
    "EFFECT_ADJUST_PLOT_PURCHASE_COST": "{Amount%} tile purchase cost",
    "EFFECT_ADJUST_PLAYER_RESOURCE_STOCKPILE_CAP": "{+Amount} strategic resource stockpile cap",
    "EFFECT_ADJUST_PLAYER_GOVERNMENT_SLOT_TYPE": "+1 {GovernmentSlotType} slot",
    "EFFECT_ADJUST_GOVERNMENT_SLOTS": "{+Amount} {GovernmentSlotType} slot(s)",
    "EFFECT_REPLACE_PLAYER_GOVERNMENT_SLOT_TYPE": "{ReplacedGovernmentSlotType} slot becomes {AddedGovernmentSlotType}{?ReplacesAll: (all of them)}",
    "EFFECT_ADJUST_GOLD_DISPERSAL": "{Amount%} Gold from clearing {Improvement}",
    "EFFECT_ADJUST_EXTRA_UNIT_COPY_TAG": "{+Amount} extra copy of {Tag} units when trained",
    "EFFECT_ADJUST_EXTRA_UNIT_COPY": "{+Amount} extra copy of {UnitType} when trained",
    "EFFECT_ADJUST_UNIT_SIGHT": "{+Amount} sight",
    "EFFECT_ADJUST_UNIT_BUILD_CHARGES": "{+Amount} build charge(s)",
    "EFFECT_ADJUST_UNIT_SPREAD_CHARGES": "{+Amount} spread charge(s)",
    "EFFECT_ADJUST_BUILDING_SPREAD_CHARGES": "religious units bought here {+Amount} spread charge(s)",
    "EFFECT_ADJUST_UNIT_ATTACK_RANGE": "{+Amount} range",
    "EFFECT_ADJUST_UNIT_NUM_ATTACKS": "{+Amount} attack(s) per turn",
    "EFFECT_ADJUST_UNIT_PLUNDER_YIELDS": "{Amount%} pillage/plunder yields",
    "EFFECT_ADJUST_UNIT_IGNORE_SHORES": "no movement cost to embark/disembark",
    "EFFECT_ADJUST_UNIT_IGNORE_RIVERS": "no river crossing penalty",
    "EFFECT_ADJUST_UNIT_FLANKING_BONUS_MODIFIER": "{Percent}% flanking bonus",
    "EFFECT_ADJUST_UNIT_SUPPORT_BONUS_MODIFIER": "{Percent}% support bonus",
    "EFFECT_ADJUST_UNIT_ATTACK_AND_MOVE": "can move after attacking",
    "EFFECT_ADJUST_UNIT_MOVE_AND_ATTACK": "can attack after moving",
    "EFFECT_GRANT_HEAL_AFTER_ACTION": "can heal after moving/attacking",
    "EFFECT_ADJUST_UNIT_SEE_THROUGH_FEATURES": "sees through features",
    "EFFECT_ADJUST_UNIT_HIDDEN_VISIBILITY": "hidden (only visible when adjacent)",
    "EFFECT_ADJUST_UNIT_ENTER_FOREIGN_LANDS": "can enter foreign territory",
    "EFFECT_ADJUST_UNIT_POST_COMBAT_HEAL": "heals {Amount} HP after killing a unit",
    "EFFECT_ADJUST_UNIT_MAINTENANCE_DISCOUNT": "{Amount} Gold unit maintenance discount",
    "EFFECT_ADJUST_UNIT_IGNORE_RESOURCE_MAINTENANCE": "no strategic resource maintenance",
    "EFFECT_ADJUST_PLAYER_UNIT_UPGRADE_DISCOUNT_PERCENT": "{Amount}% cheaper unit upgrades",
    "EFFECT_ADJUST_UNIT_BYPASS_WALLS_PROMOTION_CLASS": "{PromotionClass} units bypass walls",
    "EFFECT_ADJUST_UNIT_ENABLE_WALL_ATTACK_PROMOTION_CLASS": "{PromotionClass} units deal full damage to walls",
    "EFFECT_ADJUST_UNIT_ENABLE_WALL_ATTACK_WHOLE_GAME_PROMOTION_CLASS": "{PromotionClass} units deal full damage to walls (whole game)",
    "EFFECT_ADJUST_UNIT_ENABLE_WALL_ATTACK_WHOLE_GAME_SAME_RELIGION_PROMOTION_CLASS": "{PromotionClass} units deal full damage to walls of cities following your religion",
    "EFFECT_ADJUST_UNIT_BARBARIAN_COMBAT": "{+Amount} Combat Strength vs barbarians",
    "EFFECT_ADJUST_UNIT_DAMAGE": "{+Amount} damage",
    "EFFECT_ADJUST_UNIT_RAIDING": "can coastal raid",
    "EFFECT_ADJUST_UNIT_COMBAT_UNIT_CAPTURE": "can capture defeated units",
    "EFFECT_ADJUST_UNIT_CONVERTS_BARBARIANS": "converts defeated barbarians",
    "EFFECT_ADJUST_UNIT_MILITARY_FORMATION": "is formed as {MilitaryFormationType}",
    "EFFECT_ADJUST_UNIT_TRADE_ROUTE_PLUNDER_IMMUNITY": "trade routes immune to plunder ({DomainType})",
    "EFFECT_ADJUST_UNIT_ADVANCED_PILLAGING": "pillaging costs only 1 movement",
    "EFFECT_ADJUST_UNIT_NO_REDUCTION_DAMAGE": "no combat penalty from damage",
    "EFFECT_ADJUST_UNIT_FRIENDLY_TERRITORY_COMBAT": "{+Amount} Combat Strength in friendly territory",
    "EFFECT_ADJUST_UNIT_HOLY_CITIES_COMBAT_MODIFIER": "{+Amount} strength per converted Holy City",
    "EFFECT_ADJUST_UNIT_CLEAR_TERRAIN_START_MOVEMENT": "{+Amount} Movement when starting on open terrain",
    "EFFECT_ADJUST_UNIT_EVICT_PERCENT": "removes {Amount}% of other religions",
    "EFFECT_GRANT_PROMOTION": "grants promotion {PromotionType}",
    "EFFECT_GRANT_SPY": "grants {Amount} Spy",
    "EFFECT_GRANT_RELIC": "grants {Amount} Relic(s)",
    "EFFECT_ADJUST_FLAT_BONUS": "{Amount%} {BonusType}",
    "EFFECT_ADJUST_CITY_POPULATION": "{+Amount} Population",
    "EFFECT_ADJUST_MULTIPLY_TREASURY": "multiplies treasury by {Amount}",
    "EFFECT_ADJUST_TECHNOLOGY_BOOST": "Eurekas worth {+Amount} percentage points more",
    "EFFECT_ADJUST_CIVIC_BOOST": "Inspirations worth {+Amount} percentage points more",
    "EFFECT_ADJUST_PLAYER_TOURISM": "{Amount%} Tourism",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_TOURISM_MODIFIER": "{Amount%} Tourism to civs with your trade routes",
    "EFFECT_ADJUST_GREAT_PERSON_PATRONAGE_DISCOUNT_PERCENT": "{Amount}% cheaper Great Person patronage with {YieldType}",
    "EFFECT_ADJUST_GREAT_PEOPLE_POINTS_PER_KILL": "{+Amount} {GreatPersonClassType} points per kill",
    "EFFECT_ADJUST_GREAT_PEOPLE_POINTS_PER_KILL_BY_DEFEATED_STRENGTH": "{GreatPersonClassType} points per kill = {Amount}% of victim strength",
    "EFFECT_ADJUST_PLAYER_FREE_GREAT_PERSON_POINTS": "{+Amount} points toward every Great Person class",
    "EFFECT_ADJUST_DISTRICT_EXTRA_REGIONAL_RANGE": "{+Amount} regional range",
    "EFFECT_ADJUST_CITY_YIELD_PER_TERRAIN_TYPE": "{+Amount} {YieldType} per {TerrainType} tile",
    "EFFECT_ADJUST_YIELD_BY_NUMBER_OF_RESOURCES": "{+Amount} {YieldType} per resource",
    "EFFECT_ADJUST_RESOURCE_YIELD_BY_COUNT": "{+Amount} {YieldType} per resource copy",
    "EFFECT_GRANT_YIELD_PER_GREAT_WORK_IN_CITY": "{+Amount} {YieldType} per {GreatWorkObjectType} in the city",
    "EFFECT_GRANT_YIELD_PER_EXCESS_LUXURIES": "{+Amount} {YieldType} per surplus luxury",
    "EFFECT_ADJUST_PLAYER_YIELD_CHANGE_PER_TRIBUTARY": "{+Amount} {YieldType} per city-state you are suzerain of",
    "EFFECT_ADJUST_PLAYER_YIELD_MODIFIER_PER_TRIBUTARY": "{Amount%} {YieldType} per city-state you are suzerain of",
    "EFFECT_ADJUST_PLAYER_YIELD_CHANGE_PER_USED_INFLUENCE_TOKEN": "{+Amount} {YieldType} per Envoy sent",
    "EFFECT_ADJUST_PLAYER_YIELD_MODIFIER_PER_EARNED_GREAT_PERSON": "{Amount%} {YieldType} per Great Person earned",
    "EFFECT_ADJUST_CITY_YIELD_MODIFIER_PER_GOVERNOR_TITLE": "{Amount%} {YieldType} per Governor title",
    "EFFECT_ADJUST_CITY_YIELD_MODIFIER_FROM_FAITH": "converts {Amount}% of Faith to {YieldType}",
    "EFFECT_ADJUST_FOLLOWER_YIELD_MODIFIER": "{Amount%} {YieldType} in cities following the religion",
    "EFFECT_ADJUST_IMPROVEMENT_VALID_TERRAIN": "{ImprovementType} may be built on {TerrainType}",
    "EFFECT_ADJUST_VALID_FEATURES_DISTRICTS": "{DistrictType} may be built on {FeatureType}",
    "EFFECT_ADJUST_PLAYER_DISTRICT_CREATE_YIELD": "building {DistrictType} grants {Amount} {YieldType}",
    "EFFECT_ADJUST_PLAYER_DISTRICT_CREATE_UNIT": "building {DistrictType} grants {UnitType}",
    "EFFECT_ADJUST_PLAYER_SPECIFIC_DISTRICT_GRANT_ENVOYS": "building {DistrictType} grants {Amount} Envoy(s)",
    "EFFECT_ADJUST_GOVERNOR_IDENTITY_PRESSURE": "{+Amount} Loyalty pressure",
    "EFFECT_ADJUST_PLAYER_GRIEVANCE_DECAY": "{Amount%} grievance decay",
    "EFFECT_ADJUST_PLAYER_GRIEVANCE_GENERATION": "{Amount%} grievances generated",
    "EFFECT_ADJUST_PLAYER_LEVY_DISCOUNT_PERCENT": "{Percent}% cheaper levy",
    "EFFECT_ADJUST_NATURAL_WONDER_AMENITY": "{+Amount} Amenity per natural wonder",
    "EFFECT_ADJUST_NUM_UNITS_SUPPORTED": "{+Amount} {BuildingType} capacity",
    "EFFECT_ADJUST_AUTO_THEMED_BUILDINGS_WITH_X_SLOTS": "buildings with {Amount}+ slots are auto-themed",
    "EFFECT_ADJUST_PLAYER_ADD_CHOP_YIELD": "{Amount%} {YieldType} from harvesting/chopping",
    "EFFECT_ADJUST_PLAYER_POST_COMBAT_LOYALTY": "killing a unit changes nearby enemy city Loyalty by {Amount}",
    "EFFECT_ADJUST_PLAYER_LOYALTY_MARTIAL_LAW_MODIFIER": "{+Amount} martial law Loyalty",
    "EFFECT_ADJUST_ALLIANCE_POINTS_FOR_MODIFIER": "{+Amount} alliance points per turn",
    "EFFECT_ADJUST_WONDER_YIELD_CHANGE": "{+Amount} {YieldType} per wonder",
    "EFFECT_ADJUST_PLAYER_SCIENCE_VICTORY_POINTS_PER_TURN": "{+Amount} light-years per turn for the Exoplanet Expedition",
    "EFFECT_GRANT_AIR_SLOTS": "{+Amount} air slots",
    "EFFECT_ADJUST_PLAYER_DISTRICT_AIR_SLOTS": "{+Amount} air slots",
    "EFFECT_ADJUST_EXTRA_ACCUMALATION_TERRAIN": "{+Amount} strategic resource accumulation on {TerrainType}",
    "EFFECT_ADJUST_CITY_EXTRA_ACCUMULATION_SPECIFIC_RESOURCE": "{+Amount} {ResourceType} accumulation per source",
    "EFFECT_ADJUST_CITY_EXTRA_ACCUMULATION": "{+Amount} strategic accumulation per source",
    "EFFECT_ADJUST_DISTRICT_YIELD_BASED_ON_APPEAL": "{DistrictType} {+YieldChange} {YieldType} when appeal is at least {RequiredAppeal}",
    "EFFECT_ADJUST_PLAYER_OPEN_BORDERS_FROM_INFLUENCE": "open borders with city-states you sent an Envoy to",
    "EFFECT_TRADE_ROUTE_DISABLE": "disables trade routes",
    "EFFECT_ADJUST_DISABLE_HEALING": "units cannot heal",
    "EFFECT_DO_NOTHING": "(no effect)",
    "EFFECT_GRANT_CHEAPEST_BUILDING_IN_CITY": "grants the cheapest building of the district",
    "EFFECT_ADJUST_PLAYER_TOURISM_FAVOR": "Favor from Tourism",
    "EFFECT_ADJUST_PLAYER_BUILDING_FAVOR": "{BuildingType} grants {Favor} Favor",
    "EFFECT_GRANT_STRENGTH_PER_ADJACENT_UNIT_TYPE": "{+Amount} Combat Strength per adjacent {UnitType}",
    "EFFECT_GRANT_RELIGIOUS_PRESSURE_BURST": "{Amount} religious pressure burst within {Range} tiles",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_RELIGIOUS_PRESSURE": "{Amount%} religious pressure via trade routes",
    "EFFECT_ADJUST_RELIGIOUS_SPREAD_STRENGTH": "religious spread x{SpreadMultiplier}",
    "EFFECT_ADJUST_UNIT_INITIATION_YIELD": "{+Amount} {YieldType} when a unit is trained/bought",
    "EFFECT_ADJUST_PLAYER_BAN_CITY_PRODUCTION": "cannot produce in captured cities",
    "EFFECT_ADJUST_PLAYER_INTERNATIONAL_TRADE_ROUTE_YIELD_MODIFIER": "{Amount%} {YieldType} from international routes",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_POST_IN_FOREIGN_CITY": "{+Amount} {YieldType} per Trading Post in foreign cities",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_POST_IN_OWN_CITY": "{+Amount} {YieldType} per Trading Post in own cities",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_FOLLOWER": "{+Amount} {YieldType} per follower",
    "EFFECT_ADJUST_PLAYER_TRADE_ROUTE_YIELD_PER_PATH_TILE": "{+Amount} {YieldType} per tile of route",
    "EFFECT_ADJUST_PLAYER_INTERNATIONAL_TRADE_ROUTE_YIELD_PER_IMPROVEMENT_IN_ORIGIN_CITY": "international routes {+Amount} {YieldType} per {ImprovementType} in origin",
    "EFFECT_PLAYER_ADJUST_YIELD_FROM_EMBASSIES": "{+Amount} {YieldType} per embassy",
    "EFFECT_PLAYER_ADJUST_YIELD_FROM_DELEGATIONS": "{+Amount} {YieldType} per delegation",
    "EFFECT_ADJUST_PLAYER_WMD_COUNT": "{+Amount} {Type}",
    "EFFECT_ADJUST_GLOBAL_WMD_STOCKPILE": "{Amount%} nuclear stockpile",
    "EFFECT_ADJUST_PLAYER_OTHER_GOVERNMENT_INTOLERANCE": "intolerance of other governments x{IntoleranceMultiplier}",
    "EFFECT_ADJUST_PLAYER_EMERGENCY_FAVOR_MODIFIER": "{Amount%} Favor from emergencies",
    "EFFECT_ADJUST_PLAYER_FAVOR_REFUND_FOR_SUCCESSFUL_RESOLUTION": "{Percent}% Favor refund when your resolution passes",
    "EFFECT_PLAYER_SEND_GOLD_TO_EMERGENCIES_OF_TYPE": "send {Amount} Gold to {EmergencyType}",
    "EFFECT_ADJUST_PLAYER_ROCK_BAND_UNIT_ALBUM_SALES": "{Amount%} Rock Band album sales",
    "EFFECT_ADJUST_UNIT_WMD_PROTECTION": "protected from nuclear blast/fallout",
    "EFFECT_ADJUST_DIPLOMATIC_ACTION_PREFERENCE": "AI preference for {Action}",
    "EFFECT_ADD_DIPLOMATIC_ACTION_OVERRIDE": "unlocks {DiplomaticAction} with {CivicType}",
    "EFFECT_ADD_DIPLOMATIC_MOVEMENT_MODIFIER": "{+Amount} Movement for {TurnsActive} turns ({DiplomaticYieldSource})",
    "EFFECT_ADD_DIPLOMATIC_COMBAT_MODIFIER": "{+Amount} Combat Strength for {TurnsActive} turns ({DiplomaticYieldSource})",
    "EFFECT_ADJUST_PLAYER_POST_PILLAGE_LOYALTY": "pillaging changes nearby city Loyalty by {Amount}",
    "EFFECT_ADJUST_UNIT_PILLAGE_IMPROVEMENT_MODIFIER": "{Amount%} pillage yield from improvements",
    "EFFECT_ADJUST_UNIT_PILLAGE_DISTRICT_MODIFIER": "{Amount%} pillage yield from districts",
    "EFFECT_ADJUST_IMPROVEMENT_GOODY_HUT": "{ImprovementType} acts as goody hut ({GoodyHutImprovementType})",
    "EFFECT_ADJUST_IDENTITY_PER_TURN_FROM_NEARBY_GREAT_WORKS": "{+Amount} Loyalty per nearby Great Work",
    "EFFECT_ADJUST_FEATURE_PREREQ": "{FeatureType} unlocked by {CivicType}",
    "EFFECT_ADJUST_DISTRICT_PREREQ": "{DistrictType} unlocked by {TechType}",
    "EFFECT_ADJUST_ALLIANCE_PLAYER_STRENGTH_MODIFIER": "{+Amount} Combat Strength (alliance)",
    "EFFECT_ADJUST_PLAYER_BUFF_UNIT_PRODUCTION_YIELD": "{Amount%} unit production",
    "EFFECT_ADJUST_PLAYER_RANDOM_TECHNOLOGY_BOOST_GOODY_HUT": "{+Amount} Eureka from {Source}",
    "EFFECT_ADJUST_PLAYER_RANDOM_CIVIC_BOOST_GOODY_HUT": "{+Amount} Inspiration from {Source}",
    "EFFECT_ADJUST_GOVERNOR_GRIEVENCE_SCORE": "grievances {Score} for {Turns} turns",
    "EFFECT_ADJUST_PLAYER_GOVERNMENT_SLOT_TYPE_GRANT_FAVOR": "{+Amount} Favor per {GovernmentSlotType} slot",
    "EFFECT_ADJUST_CITY_HOUSING_FROM_GREAT_PEOPLE": "{+Amount} Housing per Great Person",
    "EFFECT_ADJUST_RELIGION_AMENITIES_FOR_MINIMUM_FOLLOWERS": "+{Amenities} Amenities in cities with at least {Followers} followers",
    "EFFECT_GRANT_ROUTE_IN_RADIUS": "roads within {Radius} tiles",
    "EFFECT_ADJUST_UNIT_LAND_VICTORY_SPREAD": "kills spread religion",
    "EFFECT_ADJUST_UNIT_DIPLO_VISIBILITY_COMBAT_MODIFIER": "{+Amount} Combat Strength per diplomatic visibility level advantage",
    "EFFECT_ADJUST_PLAYER_COUNTER_SPY_YIELD_AWARD_PER_LEVEL": "{+Amount} {YieldType} per {PerXLevels} spy levels on counterspy",
    "EFFECT_ADJUST_PLAYER_TARGET_CITY_SPY_YIELD_PERCENT": "{Percent}% {YieldType} stolen by spies",
    "EFFECT_GRANT_BOOST_WITH_GREAT_PERSON": "boost from {GreatPersonClass}",
    "EFFECT_ADJUST_UNIT_TOURISM_BOMB_IMPROVEMENT": "{+Amount} Tourism burst at {ImprovementType}",
    "EFFECT_ADJUST_UNIT_ROCK_BAND_LEVEL_IMPROVEMENT": "{+Amount} Rock Band level at {ImprovementType}",
    "EFFECT_ADJUST_UNIT_YIELD_PER_TOURISM_BOMB": "{+Amount} {YieldType} per concert",
    "EFFECT_ADJUST_UNIT_TOURISM_BOMB_RANGE": "concert range {Range}",
    "EFFECT_ADJUST_UNIT_SPY_OFFENSIVE_OPERATION_TIME": "offensive spy ops {ReductionPercent}% faster",
    "EFFECT_ADJUST_UNIT_SPY_ESTABLISH_TIME": "spies establish {ReductionPercent}% faster",
    "EFFECT_ADJUST_UNIT_BOOST_ALL_SPIES": "{+Amount} level to all spies",
    "EFFECT_ADJUST_RANDOM_EVENT_MODIFIED_DAMAGE_OPPOSING_PLAYER": "{Amount%} damage from {RandomEventType} to enemies",
    "EFFECT_ADJUST_PLAYER_RANDOM_EVENT_AVOID": "immune to {RandomEventType}",
    "EFFECT_TRIGGER_GAME_MECHANIC": "triggers {MechanicName}",
    "EFFECT_ADJUST_UNIT_HEALING_RELIGION_MODIFIERS": "{+Amount} healing ({Type})",
    "EFFECT_GRANT_UNIT_OF_CLASS_AND_APPLY_ABILITY": "grants a {UnitPromotionClassType} unit",
    "EFFECT_GRANT_UNIT_BY_CLASS": "grants a {UnitPromotionClassType} unit",
    "EFFECT_GRANT_UNIT_TYPE_UNLIMITED_PROMOTION_CHOICES": "{UnitType} may choose any promotion",
    "EFFECT_GRANT_PLAYER_YIELD_PERCENT_UNIT_COST": "{YieldType} = {UnitCostPercent}% of a unit's cost",
    "EFFECT_ADJUST_PLAYER_FEAUTE_REQUIRED_FOR_SPECIALTY_DISTRICTS": "specialty districts require {FeatureType}",
    "EFFECT_GRANT_FREE_RESOURCE_FROM_UNIT_PLOT": "grants {Amount} of the resource on the unit's tile",
    "EFFECT_ALLIANCE_YIELD_INCOME_FROM_ALLY_RELIGION": "{+Amount} {YieldType} from ally's religion",
    "EFFECT_ADJUST_WONDER_ADJACENT_NATURAL_WONDER_PRODUCTION": "{Amount%} wonder Production next to {FeatureType}",
    "EFFECT_ADJUST_PLAYER_DISTRICT_AND_BUILDINGS_CREATE_UNIT_WITH_ABILITY_BY_CLASS": "{UnitPromotionClass} units from {DistrictType} get {UnitAbilityType}",
    "EFFECT_ADJUST_UNIT_INITIATION_YIELD_POPULATION": "{+Amount} {YieldType} per pop when unit trained",
}

COLLECTIONS = {
    "COLLECTION_OWNER": "",
    "COLLECTION_PLAYER_CITIES": "in all your cities",
    "COLLECTION_MAJOR_PLAYERS": "for all major civs",
    "COLLECTION_ALL_PLAYERS": "for all players",
    "COLLECTION_PLAYER_UNITS": "for your units",
    "COLLECTION_UNIT_COMBAT": "in combat",
    "COLLECTION_ALL_CITIES": "in all cities",
    "COLLECTION_PLAYER_DISTRICTS": "for your districts",
    "COLLECTION_PLAYER_PLOT_YIELDS": "on your tiles",
    "COLLECTION_CITY_PLOT_YIELDS": "on this city's tiles",
    "COLLECTION_PLAYER_CAPITAL_CITY": "in your capital",
    "COLLECTION_CITY_DISTRICTS": "for this city's districts",
    "COLLECTION_SINGLE_PLOT_YIELDS": "on this tile",
    "COLLECTION_ALL_UNITS": "for all units",
    "COLLECTION_CITY_TRAINED_UNITS": "for units trained in this city",
    "COLLECTION_PLAYER_TRAINED_UNITS": "for units you train",
    "COLLECTION_PLAYER_CAPTURED_CITIES": "in captured cities",
    "COLLECTION_PLAYER_COMBAT": "in combat",
    "COLLECTION_UNIT_NEAREST_OWNER_CITY": "in the nearest city",
    "COLLECTION_OWNER_CITY": "in this city",
    "COLLECTION_PLAYER_BUILT_CITIES": "in cities you founded",
    "COLLECTION_ALL_DISTRICTS": "for all districts",
    "COLLECTION_PLAYER_GOVERNORS": "for your governors",
    "COLLECTION_PLAYER_CITY_STATE_UNITS": "for levied city-state units",
    "COLLECTION_EMERGENCY_PLAYERS": "for emergency members",
    "COLLECTION_EMERGENCY_CITIES": "in emergency members' cities",
    "COLLECTION_EMERGENCY_UNITS": "for emergency members' units",
    "COLLECTION_EMERGENCY_COMBATS": "in emergency combat",
    "COLLECTION_ALLIANCE_TRADEROUTES": "for trade routes with the ally",
    "COLLECTION_ALLIANCE_COMBATS": "in combat (alliance)",
}

REQUIREMENTS = {
    "REQUIREMENT_CITY_HAS_BUILDING": "city has {BuildingType}",
    "REQUIREMENT_PLAYER_HAS_BUILDING": "player has {BuildingType}",
    "REQUIREMENT_PLAYER_HAS_TECHNOLOGY": "has {TechnologyType}",
    "REQUIREMENT_PLAYER_HAS_CIVIC": "has {CivicType}",
    "REQUIREMENT_OPPONENT_UNIT_PROMOTION_CLASS_MATCHES": "vs {UnitPromotionClass}",
    "REQUIREMENT_OPPONENT_UNIT_DOMAIN_MATCHES": "vs {UnitDomain} units",
    "REQUIREMENT_OPPONENT_UNIT_TAG_MATCHES": "vs {Tag} units",
    "REQUIREMENT_OPPONENT_IS_DISTRICT": "vs a district/city",
    "REQUIREMENT_PLOT_FEATURE_TYPE_MATCHES": "tile is {FeatureType}",
    "REQUIREMENT_PLOT_TERRAIN_TYPE_MATCHES": "tile is {TerrainType}",
    "REQUIREMENT_PLOT_TERRAIN_CLASS_MATCHES": "tile is {TerrainClass}",
    "REQUIREMENT_PLOT_IMPROVEMENT_TYPE_MATCHES": "tile has {ImprovementType}",
    "REQUIREMENT_PLOT_RESOURCE_TYPE_MATCHES": "tile has {ResourceType}",
    "REQUIREMENT_PLOT_RESOURCE_CLASS_TYPE_MATCHES": "tile has a {ResourceClassType} resource",
    "REQUIREMENT_PLOT_DISTRICT_TYPE_MATCHES": "tile is {DistrictType}",
    "REQUIREMENT_DISTRICT_TYPE_MATCHES": "district is {DistrictType}",
    "REQUIREMENT_CITY_HAS_DISTRICT": "city has {DistrictType}",
    "REQUIREMENT_PLOT_ADJACENT_DISTRICT_TYPE_MATCHES": "adjacent to {DistrictType}",
    "REQUIREMENT_PLOT_ADJACENT_FEATURE_TYPE_MATCHES": "adjacent to {FeatureType}",
    "REQUIREMENT_PLOT_ADJACENT_TERRAIN_TYPE_MATCHES": "adjacent to {TerrainType}",
    "REQUIREMENT_PLOT_ADJACENT_IMPROVEMENT_TYPE_MATCHES": "adjacent to {ImprovementType}",
    "REQUIREMENT_PLOT_ADJACENT_BUILDING_TYPE_MATCHES": "within {MaxRange} tiles of {BuildingType}",
    "REQUIREMENT_PLOT_ADJACENT_RESOURCE_TYPE_MATCHES": "adjacent to {ResourceType}",
    "REQUIREMENT_PLOT_ADJACENT_RIVER": "adjacent to a river",
    "REQUIREMENT_PLOT_IS_RIVER_ADJACENT": "adjacent to a river",
    "REQUIREMENT_PLOT_ADJACENT_TO_RIVER": "adjacent to a river",
    "REQUIREMENT_PLOT_IS_COASTAL_LAND": "coastal land tile",
    "REQUIREMENT_PLOT_IS_HILLS": "tile is Hills",
    "REQUIREMENT_PLOT_IS_MOUNTAIN": "tile is a Mountain",
    "REQUIREMENT_PLOT_IS_LAKE": "tile is a lake",
    "REQUIREMENT_PLOT_IS_WATER": "water tile",
    "REQUIREMENT_PLOT_IS_FRESH_WATER": "has fresh water",
    "REQUIREMENT_PLOT_ADJACENT_FRIENDLY_UNIT_TAG_MATCHES": "adjacent to a friendly {Tag} unit",
    "REQUIREMENT_PLOT_ADJACENT_FRIENDLY_UNIT_TYPE_MATCHES": "adjacent to a friendly {UnitType}",
    "REQUIREMENT_PLOT_NEARBY_UNIT_TAG_MATCHES": "within {MaxDistance} tiles of a {Tag} unit",
    "REQUIREMENT_PLOT_ADJACENT_TO_OWNER": "within {MaxDistance} tiles",
    "REQUIREMENT_PLOT_ADJACENT_FRIENDLY_TERRITORY": "in or next to friendly territory",
    "REQUIREMENT_PLOT_NEAR_CAPITAL": "within {MaxDistance} tiles of the capital",
    "REQUIREMENT_PLOT_IS_APPEAL_BETWEEN": "appeal at least {MinimumAppeal}",
    "REQUIREMENT_PLOT_UNIT_TYPE_MATCHES": "{UnitType} on the tile",
    "REQUIREMENT_UNIT_DOMAIN_MATCHES": "{UnitDomain} unit",
    "REQUIREMENT_UNIT_TYPE_MATCHES": "unit is {UnitType}",
    "REQUIREMENT_UNIT_PROMOTION_CLASS_MATCHES": "unit is {UnitPromotionClass}",
    "REQUIREMENT_UNIT_TAG_MATCHES": "unit has tag {Tag}",
    "REQUIREMENT_UNIT_ERA_TYPE_MATCHES": "unit era {EraType}",
    "REQUIREMENT_UNIT_IN_OWNER_TERRITORY": "in own territory",
    "REQUIREMENT_UNIT_IS_LEVIED": "unit is levied",
    "REQUIREMENT_UNIT_ON_HOME_CONTINENT": "on home continent",
    "REQUIREMENT_UNIT_ON_COAST": "on a coastal tile",
    "REQUIREMENT_PLAYER_IS_SUZERAIN_X_TYPE": "suzerain of a {LeaderType} city-state (x{Amount})",
    "REQUIREMENT_PLAYER_IS_SUZERAIN": "is suzerain",
    "REQUIREMENT_PLAYER_HAS_GIVEN_INFLUENCE_TOKENS": "has sent at least {MinimumTokens} Envoys",
    "REQUIREMENT_PLAYER_HAS_GREAT_PERSON": "has {GreatPersonIndividual}",
    "REQUIREMENT_PLAYER_HAS_RESOURCE_OWNED": "owns {ResourceType}",
    "REQUIREMENT_PLAYER_HAS_RESOURCE_IMPROVED": "has improved {ResourceType}",
    "REQUIREMENT_CITY_HAS_RESOURCE_TYPE_IMPROVED": "city has improved {ResourceType}",
    "REQUIREMENT_PLAYER_HAS_COMPLETED_PROJECT": "completed {ProjectType}",
    "REQUIREMENT_PLAYER_HAS_AT_LEAST_NUMBER_CITIES": "has at least {Amount} cities",
    "REQUIREMENT_PLAYER_HANDICAP_AT_OR_ABOVE": "difficulty at or above {Handicap}",
    "REQUIREMENT_PLAYER_IS_AI": "player is AI",
    "REQUIREMENT_PLAYER_IS_HUMAN": "player is human",
    "REQUIREMENT_PLAYER_IS_MAJOR_CIV": "major civ",
    "REQUIREMENT_PLAYER_IS_ATTACKING": "when attacking",
    "REQUIREMENT_PLAYER_HAS_ACTIVE_ALLIANCE_OF_AT_LEAST_LEVEL": "has an alliance of level {Level}+",
    "REQUIREMENT_GAME_ERA_IS": "game era is {EraType}",
    "REQUIREMENT_GAME_ERA_ATLEAST_EXPANSION": "game era at least {EraType}",
    "REQUIREMENT_CITY_IS_ORIGINAL_OWNER": "city founded by owner",
    "REQUIREMENT_CITY_HAS_X_SPECIALTY_DISTRICTS": "city has {Amount}+ specialty districts",
    "REQUIREMENT_CITY_HAS_X_POPULATION": "city population {Amount}+",
    "REQUIREMENT_CITY_HAS_GOVERNOR": "city has a governor",
    "REQUIREMENT_CITY_HAS_SPECIFIC_GOVERNOR_PROMOTION_TYPE": "city has governor with {GovernorPromotionType}",
    "REQUIREMENT_CITY_HAS_GARRISON_UNIT": "city has a garrison",
    "REQUIREMENT_CITY_OCCUPIED": "city is occupied",
    "REQUIREMENT_CITY_IS_OWNER_CAPITAL_CONTINENT": "city on capital's continent",
    "REQUIREMENT_CITY_HAS_X_TERRAIN_TYPE": "city has {Amount}+ {TerrainType} tiles",
    "REQUIREMENT_CITY_HAS_HIGH_ADJACENCY_DISTRICT": "city has {DistrictType} with {Amount}+ {YieldType} adjacency",
    "REQUIREMENT_CITY_HAS_GOVERNMENT_BUILDING_TIER": "city has tier {GovernmentBuildingTier} government building",
    "REQUIREMENT_CITY_IS_COASTAL": "coastal city",
    "REQUIREMENT_CITY_FOLLOWS_RELIGION": "city follows your religion",
    "REQUIREMENT_CITY_FOLLOWS_PANTHEON": "city follows your pantheon",
    "REQUIREMENT_MAP_HAS_FEATURE": "map has {FeatureType}",
    "REQUIREMENT_PLAYER_DECLARED_WAR": "declared {WarType}",
    "REQUIREMENT_PLAYER_HAS_SAME_GOVERNMENT": "same government",
    "REQUIREMENT_UNIT_PLOT_HAS_NATIONAL_PARK": "near a National Park",
    "REQUIREMENT_PLAYER_GOVERNMENT_IS": "government is {GovernmentType}",
}


def _fmt_num(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return str(v)
    return str(int(f)) if f == int(f) else f"{f:g}"


def _signed(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return str(v)
    s = _fmt_num(v)
    return s if f < 0 else "+" + s


_PH = re.compile(r"\{(\?[A-Za-z0-9_]+:(?:[^{}]|\{[^{}]*\})*|[+]?[A-Za-z0-9_]+%?)\}")


class Renderer:
    def __init__(self, db: sqlite3.Connection, names: Names):
        self.db = db
        self.n = names
        self.mods = {r[0]: r for r in db.execute(
            "SELECT ModifierId, ModifierType, RunOnce, Permanent, OwnerRequirementSetId, SubjectRequirementSetId FROM Modifiers")}
        self.dyn = {r[0]: (r[1], r[2]) for r in db.execute("SELECT ModifierType, CollectionType, EffectType FROM DynamicModifiers")}
        self.args: dict[str, list] = {}
        for mid, name, typ, val, extra, sec in db.execute(
                "SELECT ModifierId, Name, Type, Value, Extra, SecondExtra FROM ModifierArguments"):
            self.args.setdefault(mid, []).append((name, typ, val, extra, sec))
        self.req_sets = {r[0]: r[1] for r in db.execute("SELECT RequirementSetId, RequirementSetType FROM RequirementSets")}
        self.set_reqs: dict[str, list] = {}
        for sid, rid in db.execute("SELECT RequirementSetId, RequirementId FROM RequirementSetRequirements"):
            self.set_reqs.setdefault(sid, []).append(rid)
        self.reqs = {r[0]: r for r in db.execute("SELECT RequirementId, RequirementType, Inverse FROM Requirements")}
        self.req_args: dict[str, dict] = {}
        for rid, name, val in db.execute("SELECT RequirementId, Name, Value FROM RequirementArguments"):
            self.req_args.setdefault(rid, {})[name] = val
        self.ability_mods: dict[str, list] = {}
        for a, m in db.execute("SELECT UnitAbilityType, ModifierId FROM UnitAbilityModifiers"):
            self.ability_mods.setdefault(a, []).append(m)

    # -------------------------------------------------------------- values --
    def value(self, v):
        if v is None:
            return ""
        if isinstance(v, str) and re.fullmatch(r"[A-Z][A-Z0-9_]+", v):
            return self.n(v)
        if isinstance(v, str) and v.lower() == "true":
            return "yes"
        if isinstance(v, str) and v.lower() == "false":
            return "no"
        return _fmt_num(v)

    def fill(self, template: str, args: dict, top: bool = True) -> str:
        def rep(m):
            key = m.group(1)
            if key.startswith("?"):
                name, _, text = key[1:].partition(":")
                v = args.get(name)
                if v in (None, "", "0", 0, "false", "False"):
                    return ""
                return self.fill(text, args, top=False)
            pct = key.endswith("%")
            sign = key.startswith("+")
            name = key.strip("+%")
            v = args.get(name)
            if v is None:
                return "?" if name in ("Amount",) else ""
            if pct:
                return _signed(v) + "%"
            if sign:
                return _signed(v)
            return self.value(v)
        out = _PH.sub(rep, template)
        out = re.sub(r"\s+", " ", out)
        return out.strip() if top else out

    # -------------------------------------------------------- requirements --
    def requirement(self, rid: str) -> str:
        r = self.reqs.get(rid)
        if r is None:
            return humanize(rid)
        _, rtype, inverse = r
        args = self.req_args.get(rid, {})
        if rtype == "REQUIREMENT_REQUIREMENTSET_IS_MET":
            text = self.requirement_set(args.get("RequirementSetId")) or humanize(rid)
        elif rtype in REQUIREMENTS:
            text = self.fill(REQUIREMENTS[rtype], args)
        else:
            base = rtype.replace("REQUIREMENT_", "").replace("_", " ").lower()
            if args:
                base += " (" + ", ".join(f"{k}={self.value(v)}" for k, v in args.items()) + ")"
            text = base
        return ("NOT " + text) if inverse else text

    def requirement_set(self, sid) -> str:
        if not sid:
            return ""
        reqs = self.set_reqs.get(sid, [])
        if not reqs:
            return humanize(sid).lower()
        joiner = " or " if self.req_sets.get(sid) == "REQUIREMENTSET_TEST_ANY" else " and "
        return joiner.join(self.requirement(r) for r in reqs)

    # ------------------------------------------------------------ modifiers --
    def modifier_args(self, mid) -> dict:
        out = {}
        for name, typ, val, extra, sec in self.args.get(mid, []):
            if typ == "LinearScaleFromDefaultHandicap":
                out[name] = f"{_fmt_num(val)} {_signed(extra)}/difficulty level"
            elif typ == "ScaleByGameSpeed":
                out[name] = f"{_fmt_num(val)} (x game speed)"
            else:
                out[name] = val
        return out

    def modifier(self, mid: str, depth: int = 0, show_collection=True) -> str:
        m = self.mods.get(mid)
        if m is None:
            return f"[missing modifier {mid}]"
        _, mtype, run_once, permanent, owner_rs, subject_rs = m
        coll, effect = self.dyn.get(mtype, (None, None))
        args = self.modifier_args(mid)
        if effect in ("EFFECT_ATTACH_MODIFIER", "EFFECT_ATTACH_MODIFIER_TO_PLAYERTYPE",
                      "EFFECT_ATTACH_MODIFIER_IF_PROMOTION_CLASS_MATCHES") and depth < 4:
            inner = self.modifier(args.get("ModifierId"), depth + 1)
            phrase = inner
        elif effect == "EFFECT_GRANT_ABILITY" and depth < 4:
            ab = args.get("AbilityType")
            inner = "; ".join(self.modifier(x, depth + 1) for x in self.ability_mods.get(ab, []))
            phrase = f"ability {self.n(ab)}" + (f" [{inner}]" if inner else "")
        elif effect in ("EFFECT_ADJUST_UNIT_GRANT_EXPERIENCE",) and str(args.get("Amount")) == "-1":
            phrase = "grants enough XP for a promotion"
        elif effect in EFFECTS:
            if str(args.get("Experience")) == "-1":
                args["Experience"] = "enough"
            phrase = self.multi_fill(EFFECTS[effect], args)
        elif effect and effect.startswith("EFFECT_DIPLOMACY") or (effect or "").startswith("EFFECT_PLAYER_DIPLOMACY"):
            phrase = "AI diplomacy opinion modifier (" + (effect or mtype).replace("EFFECT_", "").replace("_", " ").lower() + ")"
        else:
            base = (effect or mtype or "").replace("EFFECT_", "").replace("_", " ").lower()
            if args:
                base += " (" + ", ".join(f"{k}={self.value(v)}" for k, v in args.items()) + ")"
            phrase = base
        parts = [phrase]
        if show_collection and coll in COLLECTIONS and COLLECTIONS[coll]:
            parts.append(COLLECTIONS[coll])
        elif show_collection and coll and coll not in COLLECTIONS:
            parts.append("for " + coll.replace("COLLECTION_", "").replace("_", " ").lower())
        sub = self.requirement_set(subject_rs)
        if sub:
            parts.append(f"where {sub}")
        own = self.requirement_set(owner_rs)
        if own:
            parts.append(f"(while {own})")
        if run_once:
            parts.append("(one-time)")
        return " ".join(p for p in parts if p)

    def multi_fill(self, template, args):
        """Some arguments hold parallel comma lists (YieldType=A,B Amount=1,2)."""
        lists = {k: v.split(",") for k, v in args.items() if isinstance(v, str) and "," in v}
        if not lists:
            return self.fill(template, args)
        n = max(len(v) for v in lists.values())
        parts = []
        for i in range(n):
            a = dict(args)
            for k, v in lists.items():
                a[k] = v[i] if i < len(v) else v[-1]
            parts.append(self.fill(template, a))
        return ", ".join(parts)

    def modifiers(self, mids) -> str:
        seen, out = set(), []
        for m in mids:
            t = self.modifier(m)
            if t not in seen:
                seen.add(t)
                out.append(t)
        return "; ".join(out)
