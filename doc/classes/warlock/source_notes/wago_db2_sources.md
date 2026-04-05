# Wago DB2 Source Documentation -- Warlock Extraction Pipeline

## Data Source

- **Build**: 12.0.1.66709 (Midnight/TWW)
- **CSV Directory**: `wago/merged_csv/12.0.1.66709/enUS/`
- **Loaded via**: `wago_common.load_wago_csv()` / `load_wago_csv_by_key()`

## Tables Used

| Table | Purpose | Key Columns |
|-------|---------|-------------|
| `ChrSpecialization` | Warlock spec IDs (265/266/267) | `ID`, `ClassID`, `Name_lang` |
| `SpecSetMember` | Links SpecSet -> ChrSpecialization | `SpecSet`, `ChrSpecializationID` |
| `SkillLine` | Warlock skill line (849) | `ID`, `DisplayName_lang` |
| `SkillLineXTraitTree` | Links SkillLine -> TraitTree | `SkillLineID`, `TraitTreeID` |
| `TraitTree` | Warlock talent tree (720) | `ID`, `FirstTraitNodeID` |
| `TraitNode` | 199 talent nodes in tree 720 | `ID`, `TraitTreeID`, `PosX`, `PosY`, `Type`, `TraitSubTreeID` |
| `TraitNodeXTraitNodeEntry` | Links nodes -> entries | `TraitNodeID`, `TraitNodeEntryID`, `Index` |
| `TraitNodeEntry` | Entry metadata (max ranks, type) | `ID`, `TraitDefinitionID`, `MaxRanks`, `NodeEntryType` |
| `TraitDefinition` | Spell mapping per entry | `ID`, `SpellID`, `OverridesSpellID`, `VisibleSpellID`, `OverrideName_lang` |
| `TraitDefinitionEffectPoints` | Rank scaling curves | `TraitDefinitionID`, `EffectIndex`, `CurveID` |
| `TraitEdge` | Node dependencies | `LeftTraitNodeID` (parent), `RightTraitNodeID` (child), `Type` |
| `TraitSubTree` | Hero talent trees | `ID`, `Name_lang`, `TraitTreeID` |
| `TraitNodeGroup` | Groups of nodes for conditions | `ID`, `TraitTreeID` |
| `TraitNodeGroupXTraitNode` | Links groups -> nodes | `TraitNodeGroupID`, `TraitNodeID` |
| `TraitNodeGroupXTraitCond` | Links groups -> conditions | `TraitNodeGroupID`, `TraitCondID` |
| `TraitCond` | Conditions (spec gating) | `ID`, `SpecSetID`, `TraitTreeID` |
| `SpellName` | Spell ID -> name resolution | `ID`, `Name_lang` |

## Key IDs

| Entity | ID | Name |
|--------|-----|------|
| Class | 9 | Warlock |
| SkillLine | 849 | Warlock |
| TraitTree | 720 | Warlock talent tree |
| ChrSpec | 265 | Affliction |
| ChrSpec | 266 | Demonology |
| ChrSpec | 267 | Destruction |
| SpecSet | 37 | Affliction spec gate |
| SpecSet | 38 | Destruction spec gate |
| SpecSet | 39 | Demonology spec gate |
| SubTree | 57 | Soul Harvester |
| SubTree | 58 | Hellcaller |
| SubTree | 59 | Diabolist |

## Data Flow

```
ChrSpecialization (ClassID=9) -> SpecSetMember -> SpecSet IDs
SkillLine (849) -> SkillLineXTraitTree -> TraitTree (720)
TraitTree (720) -> TraitNode (199 nodes)
TraitNode -> TraitNodeXTraitNodeEntry -> TraitNodeEntry -> TraitDefinition -> SpellName
TraitNode -> TraitEdge (278 dependency edges)
TraitNode -> TraitSubTree (3 hero trees: 57/58/59)
TraitNodeGroup -> TraitNodeGroupXTraitCond -> TraitCond (SpecSetID) -> spec gating
```

## Filtered Out

- 3 placeholder nodes (spellId=0 on all entries, PosX 6900-8700) -- empty Blizzard slots
