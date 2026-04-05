"""
warlock_constants.py -- Canonical Warlock IDs from DB2 data (build 66709).

All values verified against Wago DB2 CSVs:
- ChrSpecialization, SpecSetMember, TraitTree, TraitSubTree, SkillLineXTraitTree
"""

# Class
WARLOCK_CLASS_ID = 9

# ChrSpecialization IDs
SPEC_AFFLICTION = 265
SPEC_DEMONOLOGY = 266
SPEC_DESTRUCTION = 267
SPEC_INITIAL = 1454  # pre-spec-selection leveling

# SpecSet IDs (used in TraitCond for spec-gating nodes)
SPECSET_AFFLICTION = 37   # -> ChrSpecializationID 265
SPECSET_DESTRUCTION = 38  # -> ChrSpecializationID 267
SPECSET_DEMONOLOGY = 39   # -> ChrSpecializationID 266

# Mastery spells
MASTERY_AFFLICTION = 77215
MASTERY_DEMONOLOGY = 77219
MASTERY_DESTRUCTION = 77220

# SkillLine
SKILLLINE_WARLOCK = 849

# TraitTree (single tree for all warlock specs)
TRAIT_TREE_WARLOCK = 720

# TraitSubTree IDs (hero talent trees)
SUBTREE_SOUL_HARVESTER = 57
SUBTREE_HELLCALLER = 58
SUBTREE_DIABOLIST = 59

# Reverse maps for labeling
SPECSET_TO_SPEC = {
    SPECSET_AFFLICTION: "affliction",
    SPECSET_DESTRUCTION: "destruction",
    SPECSET_DEMONOLOGY: "demonology",
}

SUBTREE_TO_HERO = {
    SUBTREE_SOUL_HARVESTER: "hero_soul_harvester",
    SUBTREE_HELLCALLER: "hero_hellcaller",
    SUBTREE_DIABOLIST: "hero_diabolist",
}

SPEC_ID_TO_NAME = {
    SPEC_AFFLICTION: "affliction",
    SPEC_DEMONOLOGY: "demonology",
    SPEC_DESTRUCTION: "destruction",
}

# Hero subtree -> applicable specs (from retail talent UI)
HERO_SPEC_ELIGIBILITY = {
    "hero_diabolist": ["demonology", "destruction"],
    "hero_hellcaller": ["affliction", "destruction"],
    "hero_soul_harvester": ["affliction", "demonology"],
}

# TraitNode.Type values
NODE_TYPE_REGULAR = 0
NODE_TYPE_SELECTION = 2  # choice node (pick one of N entries)

# TraitEdge.Type values
EDGE_TYPE_DEFAULT = 0
EDGE_TYPE_REQUIRED = 2  # must unlock parent before child
