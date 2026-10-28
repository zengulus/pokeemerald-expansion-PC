#include "config/item.h"
	.include "asm/macros.inc"
	.include "constants/constants.inc"

	.section script_data, "aw", %progbits

	.align 2
gFieldEffectScriptPointers::
	data_ptr gFieldEffectScript_ExclamationMarkIcon1      @ FLDEFF_EXCLAMATION_MARK_ICON
	data_ptr gFieldEffectScript_UseCutOnTallGrass         @ FLDEFF_USE_CUT_ON_GRASS
	data_ptr gFieldEffectScript_UseCutOnTree              @ FLDEFF_USE_CUT_ON_TREE
	data_ptr gFieldEffectScript_Shadow                    @ FLDEFF_SHADOW
	data_ptr gFieldEffectScript_TallGrass                 @ FLDEFF_TALL_GRASS
	data_ptr gFieldEffectScript_Ripple                    @ FLDEFF_RIPPLE
	data_ptr gFieldEffectScript_FieldMoveShowMon          @ FLDEFF_FIELD_MOVE_SHOW_MON
	data_ptr gFieldEffectScript_Ash                       @ FLDEFF_ASH
	data_ptr gFieldEffectScript_SurfBlob                  @ FLDEFF_SURF_BLOB
	data_ptr gFieldEffectScript_UseSurf                   @ FLDEFF_USE_SURF
	data_ptr gFieldEffectScript_GroundImpactDust          @ FLDEFF_DUST
	data_ptr gFieldEffectScript_UseSecretPowerCave        @ FLDEFF_USE_SECRET_POWER_CAVE
	data_ptr gFieldEffectScript_JumpTallGrass             @ FLDEFF_JUMP_TALL_GRASS
	data_ptr gFieldEffectScript_SandFootprints            @ FLDEFF_SAND_FOOTPRINTS
	data_ptr gFieldEffectScript_JumpBigSplash             @ FLDEFF_JUMP_BIG_SPLASH
	data_ptr gFieldEffectScript_Splash                    @ FLDEFF_SPLASH
	data_ptr gFieldEffectScript_JumpSmallSplash           @ FLDEFF_JUMP_SMALL_SPLASH
	data_ptr gFieldEffectScript_LongGrass                 @ FLDEFF_LONG_GRASS
	data_ptr gFieldEffectScript_JumpLongGrass             @ FLDEFF_JUMP_LONG_GRASS
	data_ptr gFieldEffectScript_ShakingGrass              @ FLDEFF_SHAKING_GRASS
	data_ptr gFieldEffectScript_ShakingGrass2             @ FLDEFF_SHAKING_LONG_GRASS
	data_ptr gFieldEffectScript_UnusedSand                @ FLDEFF_SAND_HOLE
	data_ptr gFieldEffectScript_WaterSurfacing            @ FLDEFF_WATER_SURFACING
	data_ptr gFieldEffectScript_BerryTreeGrowthSparkle    @ FLDEFF_BERRY_TREE_GROWTH_SPARKLE
	data_ptr gFieldEffectScript_DeepSandFootprints        @ FLDEFF_DEEP_SAND_FOOTPRINTS
	data_ptr gFieldEffectScript_PokeCenterHeal            @ FLDEFF_POKECENTER_HEAL
	data_ptr gFieldEffectScript_UseSecretPowerTree        @ FLDEFF_USE_SECRET_POWER_TREE
	data_ptr gFieldEffectScript_UseSecretPowerShrub       @ FLDEFF_USE_SECRET_POWER_SHRUB
	data_ptr gFieldEffectScript_TreeDisguise              @ FLDEFF_TREE_DISGUISE
	data_ptr gFieldEffectScript_MountainDisguise          @ FLDEFF_MOUNTAIN_DISGUISE
	data_ptr gFieldEffectScript_NPCUseFly                 @ FLDEFF_NPCFLY_OUT
	data_ptr gFieldEffectScript_UseFly                    @ FLDEFF_USE_FLY
	data_ptr gFieldEffectScript_FlyIn                     @ FLDEFF_FLY_IN
	data_ptr gFieldEffectScript_QuestionMarkIcon          @ FLDEFF_QUESTION_MARK_ICON
	data_ptr gFieldEffectScript_FeetInFlowingWater        @ FLDEFF_FEET_IN_FLOWING_WATER
	data_ptr gFieldEffectScript_BikeTireTracks            @ FLDEFF_BIKE_TIRE_TRACKS
	data_ptr gFieldEffectScript_SandDisguisePlaceholder   @ FLDEFF_SAND_DISGUISE
	data_ptr gFieldEffectScript_UseRockSmash              @ FLDEFF_USE_ROCK_SMASH
	data_ptr gFieldEffectScript_UseDig                    @ FLDEFF_USE_DIG
	data_ptr gFieldEffectScript_SandPile                  @ FLDEFF_SAND_PILE
	data_ptr gFieldEffectScript_UseStrength               @ FLDEFF_USE_STRENGTH
	data_ptr gFieldEffectScript_ShortGrass                @ FLDEFF_SHORT_GRASS
	data_ptr gFieldEffectScript_HotSpringsWater           @ FLDEFF_HOT_SPRINGS_WATER
	data_ptr gFieldEffectScript_UseWaterfall              @ FLDEFF_USE_WATERFALL
	data_ptr gFieldEffectScript_UseDive                   @ FLDEFF_USE_DIVE
	data_ptr gFieldEffectScript_PokeballTrail             @ FLDEFF_POKEBALL_TRAIL
	data_ptr gFieldEffectScript_HeartIcon                 @ FLDEFF_HEART_ICON
	data_ptr gFieldEffectScript_Nop47                     @ FLDEFF_NOP_47
	data_ptr gFieldEffectScript_Nop48                     @ FLDEFF_NOP_48
	data_ptr gFieldEffectScript_AshPuff                   @ FLDEFF_ASH_PUFF
	data_ptr gFieldEffectScript_AshLaunch                 @ FLDEFF_ASH_LAUNCH
	data_ptr gFieldEffectScript_SweetScent                @ FLDEFF_SWEET_SCENT
	data_ptr gFieldEffectScript_SandPillar                @ FLDEFF_SAND_PILLAR
	data_ptr gFieldEffectScript_Bubbles                   @ FLDEFF_BUBBLES
	data_ptr gFieldEffectScript_Sparkle                   @ FLDEFF_SPARKLE
	data_ptr gFieldEffectScript_ShowSecretPowerCave       @ FLDEFF_SECRET_POWER_CAVE
	data_ptr gFieldEffectScript_ShowSecretPowerTree       @ FLDEFF_SECRET_POWER_TREE
	data_ptr gFieldEffectScript_ShowSecretPowerShrub      @ FLDEFF_SECRET_POWER_SHRUB
	data_ptr gFieldEffectScript_ShowCutGrass              @ FLDEFF_CUT_GRASS
	data_ptr gFieldEffectScript_FieldMoveShowMonInit      @ FLDEFF_FIELD_MOVE_SHOW_MON_INIT
	data_ptr gFieldEffectScript_UsePuzzleEffect           @ FLDEFF_USE_TOMB_PUZZLE_EFFECT
	data_ptr gFieldEffectScript_SecretBaseBootPC          @ FLDEFF_PCTURN_ON
	data_ptr gFieldEffectScript_HallOfFameRecord          @ FLDEFF_HALL_OF_FAME_RECORD
	data_ptr gFieldEffectScript_UseTeleport               @ FLDEFF_USE_TELEPORT
	data_ptr gFieldEffectScript_RayquazaSpotlight         @ FLDEFF_RAYQUAZA_SPOTLIGHT
	data_ptr gFieldEffectScript_DestroyDeoxysRock         @ FLDEFF_DESTROY_DEOXYS_ROCK
	data_ptr gFieldEffectScript_MoveDeoxysRock            @ FLDEFF_MOVE_DEOXYS_ROCK
	data_ptr gFldEffScript_UseVsSeeker                    @ FLDEFF_USE_VS_SEEKER
	data_ptr gFldEffScript_XIcon                          @ FLDEFF_X_ICON
	data_ptr gFldEffScript_DoubleExclMarkIcon             @ FLDEFF_DOUBLE_EXCL_MARK_ICON
	data_ptr gFieldEffectScript_TracksSlither             @ FLDEFF_TRACKS_SLITHER
	data_ptr gFieldEffectScript_TracksBug                 @ FLDEFF_TRACKS_BUG
	data_ptr gFieldEffectScript_TracksSpot                @ FLDEFF_TRACKS_SPOT
	data_ptr gFieldEffectScript_CaveDust                  @ FLDEFF_CAVE_DUST
	data_ptr gFieldEffectScript_Defog                     @ FLDEFF_DEFOG
	data_ptr gFieldEffectScript_UseRockClimb              @ FLDEFF_USE_ROCK_CLIMB
	data_ptr gFieldEffectScript_RockClimbDust             @ FLDEFF_ROCK_CLIMB_DUST
	data_ptr gFieldEffectScript_ORASDowse                 @ FLDEFF_ORAS_DOWSE
	data_ptr gFldEffScript_SmileyFaceIcon    		        @ FLDEFF_SMILEY_FACE_ICON
	data_ptr gFieldEffectScript_HallOfFameRecordFrlg      @ FLDEFF_HALL_OF_FAME_RECORD_FRLG
	data_ptr gFldEffScript_PhotoFlash      		        @ FLDEFF_PHOTO_FLASH
	data_ptr gFieldEffectScript_OWE_SpawnAnim				@ FLDEFF_OW_ENCOUNTER_SPAWN_ANIM

gFieldEffectScript_ExclamationMarkIcon1::
	field_eff_callnative FldEff_ExclamationMarkIcon
	field_eff_end

gFieldEffectScript_UseCutOnTallGrass::
	field_eff_callnative FldEff_UseCutOnGrass
	field_eff_end

gFieldEffectScript_UseCutOnTree::
	field_eff_callnative FldEff_UseCutOnTree
	field_eff_end

gFieldEffectScript_Shadow::
	field_eff_callnative FldEff_Shadow
	field_eff_end

gFieldEffectScript_TallGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_TallGrass
	field_eff_end

gFieldEffectScript_Ripple::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_Ripple
	field_eff_end

gFieldEffectScript_FieldMoveShowMon::
	field_eff_callnative FldEff_FieldMoveShowMon
	field_eff_end

gFieldEffectScript_Ash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_Ash
	field_eff_end

gFieldEffectScript_SurfBlob::
	field_eff_callnative FldEff_SurfBlob
	field_eff_end

gFieldEffectScript_UseSurf::
	field_eff_callnative FldEff_UseSurf
	field_eff_end

gFieldEffectScript_GroundImpactDust::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Dust
	field_eff_end

gFieldEffectScript_UseSecretPowerCave::
	field_eff_callnative FldEff_UseSecretPowerCave
	field_eff_end

gFieldEffectScript_JumpTallGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_JumpTallGrass
	field_eff_end

gFieldEffectScript_SandFootprints::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_SandFootprints
	field_eff_end

gFieldEffectScript_JumpBigSplash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_JumpBigSplash
	field_eff_end

gFieldEffectScript_Splash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Splash
	field_eff_end

gFieldEffectScript_JumpSmallSplash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_JumpSmallSplash
	field_eff_end

gFieldEffectScript_LongGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_LongGrass
	field_eff_end

gFieldEffectScript_JumpLongGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_JumpLongGrass
	field_eff_end

gFieldEffectScript_ShakingGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_ShakingGrass
	field_eff_end

gFieldEffectScript_ShakingGrass2::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_ShakingGrass2
	field_eff_end

gFieldEffectScript_UnusedSand::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_UnusedSand
	field_eff_end

gFieldEffectScript_WaterSurfacing::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_WaterSurfacing
	field_eff_end

gFieldEffectScript_BerryTreeGrowthSparkle::
	field_eff_callnative FldEff_BerryTreeGrowthSparkle
	field_eff_end

gFieldEffectScript_DeepSandFootprints::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_DeepSandFootprints
	field_eff_end

gFieldEffectScript_PokeCenterHeal::
	field_eff_loadfadedpal gSpritePalette_PokeballGlow
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_PokecenterHeal
	field_eff_end

gFieldEffectScript_UseSecretPowerTree::
	field_eff_callnative FldEff_UseSecretPowerTree
	field_eff_end

gFieldEffectScript_UseSecretPowerShrub::
	field_eff_callnative FldEff_UseSecretPowerShrub
	field_eff_end

gFieldEffectScript_TreeDisguise::
	field_eff_callnative ShowTreeDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_MountainDisguise::
	field_eff_callnative ShowMountainDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_NPCUseFly::
	field_eff_callnative FldEff_NPCFlyOut
	field_eff_end

gFieldEffectScript_UseFly::
	field_eff_callnative FldEff_UseFly
	field_eff_end

gFieldEffectScript_FlyIn::
	field_eff_callnative FldEff_FlyIn
	field_eff_end

gFieldEffectScript_QuestionMarkIcon::
	field_eff_callnative FldEff_QuestionMarkIcon
	field_eff_end

gFieldEffectScript_FeetInFlowingWater::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_FeetInFlowingWater
	field_eff_end

gFieldEffectScript_BikeTireTracks::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_BikeTireTracks
	field_eff_end

gFieldEffectScript_SandDisguisePlaceholder::
	field_eff_callnative ShowSandDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_UseRockSmash::
	field_eff_callnative FldEff_UseRockSmash
	field_eff_end

gFieldEffectScript_UseStrength::
	field_eff_callnative FldEff_UseStrength
	field_eff_end

gFieldEffectScript_UseDig::
	field_eff_callnative FldEff_UseDig
	field_eff_end

gFieldEffectScript_SandPile::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_SandPile
	field_eff_end

gFieldEffectScript_ShortGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_ShortGrass
	field_eff_end

gFieldEffectScript_HotSpringsWater::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_HotSpringsWater
	field_eff_end

gFieldEffectScript_UseWaterfall::
	field_eff_callnative FldEff_UseWaterfall
	field_eff_end

gFieldEffectScript_UseDive::
	field_eff_callnative FldEff_UseDive
	field_eff_end

gFieldEffectScript_PokeballTrail::
	field_eff_loadpal gSpritePalette_Pokeball
	field_eff_callnative FldEff_PokeballTrail
	field_eff_end

gFieldEffectScript_HeartIcon::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_HeartIcon
	field_eff_end

gFieldEffectScript_Nop47::
	field_eff_callnative FldEff_Nop47
	field_eff_end

gFieldEffectScript_Nop48::
	field_eff_callnative FldEff_Nop48
	field_eff_end

gFieldEffectScript_AshPuff::
	field_eff_loadfadedpal_callnative gSpritePalette_Ash, FldEff_AshPuff
	field_eff_end

gFieldEffectScript_AshLaunch::
	field_eff_loadfadedpal_callnative gSpritePalette_Ash, FldEff_AshLaunch
	field_eff_end

gFieldEffectScript_SweetScent::
	field_eff_callnative FldEff_SweetScent
	field_eff_end

gFieldEffectScript_SandPillar::
	field_eff_loadfadedpal_callnative gSpritePalette_SandPillar, FldEff_SandPillar
	field_eff_end

gFieldEffectScript_Bubbles::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Bubbles
	field_eff_end

gFieldEffectScript_Sparkle::
	field_eff_loadfadedpal_callnative gSpritePalette_SmallSparkle, FldEff_Sparkle
	field_eff_end

gFieldEffectScript_ShowSecretPowerCave::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Cave, FldEff_SecretPowerCave
	field_eff_end

gFieldEffectScript_ShowSecretPowerTree::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Plant, FldEff_SecretPowerTree
	field_eff_end

gFieldEffectScript_ShowSecretPowerShrub::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Plant, FldEff_SecretPowerShrub
	field_eff_end

gFieldEffectScript_ShowCutGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_CutGrass, FldEff_CutGrass
	field_eff_end

gFieldEffectScript_FieldMoveShowMonInit::
	field_eff_callnative FldEff_FieldMoveShowMonInit
	field_eff_end

gFieldEffectScript_UsePuzzleEffect::
	field_eff_callnative FldEff_UsePuzzleEffect
	field_eff_end

gFieldEffectScript_SecretBaseBootPC::
	field_eff_callnative FldEff_SecretBasePCTurnOn
	field_eff_end

gFieldEffectScript_HallOfFameRecord::
	field_eff_loadfadedpal gSpritePalette_PokeballGlow
	field_eff_loadfadedpal_callnative gSpritePalette_HofMonitor, FldEff_HallOfFameRecord
	field_eff_end

gFieldEffectScript_UseTeleport::
	field_eff_callnative FldEff_UseTeleport
	field_eff_end

gFieldEffectScript_RayquazaSpotlight::
	field_eff_callnative FldEff_RayquazaSpotlight
	field_eff_end

gFieldEffectScript_DestroyDeoxysRock::
	field_eff_callnative FldEff_DestroyDeoxysRock
	field_eff_end

gFieldEffectScript_MoveDeoxysRock::
	field_eff_callnative FldEff_MoveDeoxysRock
	field_eff_end

gFldEffScript_UseVsSeeker::
	field_eff_callnative FldEff_UseVsSeeker
	field_eff_end

gFldEffScript_XIcon::
	field_eff_callnative FldEff_XIcon
	field_eff_end

gFldEffScript_DoubleExclMarkIcon::
	field_eff_callnative FldEff_DoubleExclMarkIcon
	field_eff_end

gFieldEffectScript_TracksBug::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_TracksBug
	field_eff_end

gFieldEffectScript_TracksSpot::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_TracksSpot
	field_eff_end

gFieldEffectScript_TracksSlither::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_TracksSlither
	field_eff_end

gFieldEffectScript_CaveDust::
	field_eff_loadfadedpal_callnative gSpritePalette_CaveDust FldEff_CaveDust
	field_eff_end

gFieldEffectScript_Defog::
	field_eff_callnative FldEff_Defog
	field_eff_end

gFieldEffectScript_UseRockClimb:: @ 82DBC3F
	field_eff_callnative FldEff_UseRockClimb
	field_eff_end

gFieldEffectScript_RockClimbDust:: @ 82DBB28
	field_eff_loadfadedpal_callnative gSpritePalette_BigDust, FldEff_RockClimbDust
	field_eff_end

gFieldEffectScript_ORASDowse::
	field_eff_callnative FldEff_ORASDowsing
	field_eff_end

gFldEffScript_SmileyFaceIcon::
	field_eff_callnative FldEff_SmileyFaceIcon
	field_eff_end

gFieldEffectScript_HallOfFameRecordFrlg::
	field_eff_loadfadedpal gSpritePalette_PokeballGlow
	field_eff_loadfadedpal_callnative gSpritePalette_HofMonitor_Frlg, FldEff_HallOfFameRecord
	field_eff_end

gFldEffScript_PhotoFlash::
	field_eff_callnative FldEff_PhotoFlash
	field_eff_end

gFieldEffectScript_OWE_SpawnAnim::
	field_eff_callnative FldEff_OWE_SpawnAnim
	field_eff_end
