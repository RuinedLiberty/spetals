#include <Shared/StaticData.hh>

#include <cmath>

uint32_t const MAX_LEVEL = 99;
uint32_t const TPS = 20;

// Default number of CPU-controlled player bots to spawn
uint32_t const BOT_COUNT = 20;

float const PETAL_DISABLE_DELAY = 45.0f; //seconds
float const PLAYER_ACCELERATION = 5.0f;
float const DEFAULT_FRICTION = 1.0f/3.0f;
float const SUMMON_RETREAT_RADIUS = 600.0f;
float const DIGGER_SPAWN_CHANCE = 0.25f;

float const BASE_FLOWER_RADIUS = 25.0f;
float const BASE_PETAL_ROTATION_SPEED = 2.5f;
float const BASE_FOV = 0.9f;
float const BASE_HEALTH = 100.0f;
float const BASE_BODY_DAMAGE = 25.0f;

std::array<struct PetalData, PetalID::kNumPetals> const PETAL_DATA = {{
    {
        .name = "None",
        .description = "How can you see this?",
        .health = 0.0, 
        .damage = 0.0,
        .radius = 0.0,
        .reload = 1.0,
        .count = 0,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Basic",
        .description = "A nice petal, not too strong but not too weak",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Fast",
        .description = "Weaker than most petals, but reloads very quickly",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 0.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Heavy",
        .description = "Very resilient and deals more damage, but reloads very slowly",
        .health = 20.0,
        .damage = 20.0,
        .radius = 12.0,
        .reload = 5.5,
        .count = 1,
        .rarity = RarityID::kCommon,
        .attributes = {}
    },
    {
        .name = "Stinger",
        .description = "It really hurts, but it's really fragile",
        .health = 8.0,
        .damage = 35.0,
        .radius = 7.0,
        .reload = 4.0,
        .count = 1,
        .rarity = RarityID::kUnusual,
        .attributes = {}
    },
    {
        .name = "Leaf",
        .description = "Gathers energy from the sun to passively heal your flower",
        .health = 10.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kUnusual,
        .attributes = {
            .constant_heal = 1,
            .icon_angle = -1
        }
    },
    {
        .name = "Twin",
        .description = "Why stop at one? Why not TWO?!",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.0,
        .count = 2,
        .rarity = RarityID::kUnusual,
        .attributes = {}
    },
    {
        .name = "Rose",
        .description = "Its healing properties are amazing. Not so good at combat though",
        .health = 5.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 3.5,
        .count = 1,
        .rarity = RarityID::kUnusual,
        .attributes = { 
            .secondary_reload = 1.0,
            .burst_heal = 10,
            .defend_only = 1
        }
    },
    {
        .name = "Iris",
        .description = "Very poisonous, but takes a while to do its work",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 6.0,
        .count = 1,
        .rarity = RarityID::kUnusual,
        .attributes = { 
            .poison_damage = {
                .damage = 10.0,
                .time = 6.0
            }
        }
    },
    {
        .name = "Missile",
        .description = "You can actually shoot this one",
        .health = 10.0,
        .damage = 35.0,
        .radius = 10.0,
        .reload = 3.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.5,
            .defend_only = 1,
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot 
        }
    },
    {
        .name = "Dandelion",
        .description = "Its interesting properties prevent healing effects on affected units",
        .health = 10.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.5, 
            .defend_only = 1,
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot 
        }
    },
    {
        .name = "Bubble",
        .description = "You can right click to pop it and propel your flower",
        .health = 1.0,
        .damage = 0.0,
        .radius = 12.0,
        .reload = 3.5,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.4,
            .defend_only = 1,
        }
    },
    {
        .name = "Faster",
        .description = "It's so light it makes your other petals spin faster",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 0.5,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .extra_rotation_speed = 0.8
        }
    },
    {
        .name = "Rock",
        .description = "Even more durable, but slower to recharge",
        .health = 90.0,
        .damage = 10.0,
        .radius = 12.0,
        .reload = 10.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {}
    },
    {
        .name = "Cactus",
        .description = "Not very strong, but somehow increases your maximum health",
        .health = 15.0,
        .damage = 5.0, 
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .extra_health = 20
        }
    },
    {
        .name = "Web",
        .description = "It's really sticky",
        .health = 5.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 3.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }
    },
    {
        .name = "Wing",
        .description = "It comes and goes",
        .health = 15.0,
        .damage = 15.0,
        .radius = 10.0,
        .reload = 1.25,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .icon_angle = 1,
        }
    },
    {
        .name = "Peas",
        .description = "4 in 1 deal",
        .health = 20.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.4,
        .count = 4,
        .rarity = RarityID::kRare,
        .attributes = {
            .icon_angle = 0.2,
            .clump_radius = 8,
            .secondary_reload = 0.1,
            .defend_only = 1,
            .split_projectile = 1
        }
    },
    {
        .name = "Sand",
        .description = "It's coarse, rough, and gets everywhere",
        .health = 10.0,
        .damage = 3.0,
        .radius = 7.0,
        .reload = 1.5,
        .count = 4,
        .rarity = RarityID::kRare,
        .attributes = {
            .clump_radius = 10,
        }
    },
    {
        .name = "Pincer",
        .description = "Stuns and poisons targets for a short duration",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .slow_inflict_seconds = 2,
            .poison_damage = {
                .damage = 5.0,
                .time = 1.0
            },
            .icon_angle = 0.7
        }
    },
    {
        .name = "Rose",
        .description = "Its healing properties are amazing. Not so good at combat though",
        .health = 5.0,
        .damage = 5.0,
        .radius = 7.0,
        .reload = 3.5,
        .count = 3,
        .rarity = RarityID::kRare,
        .attributes = { 
            .clump_radius = 10,
            .secondary_reload = 1.0,
            .burst_heal = 3.5,
            .defend_only = 1
        }
    },
    {
        .name = "Triplet",
        .description = "How about THREE?!",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.0,
        .count = 3,
        .rarity = RarityID::kEpic,
        .attributes = {}
    },
    {
        .name = "Egg",
        .description = "Something interesting might pop out of this",
        .health = 50.0,
        .damage = 1.0,
        .radius = 12.5,
        .reload = 1.0,
        .count = 2,
        .rarity = RarityID::kEpic,
        .attributes = { 
            .secondary_reload = 3.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kSoldierAnt
        }
    },
    
    {
        .name = "Pollen",
        .description = "Asthmatics beware",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.0,
        .count = 3,
        .rarity = RarityID::kEpic,
        .attributes = {
            .secondary_reload = 0.5,
            .defend_only = 1
        }
    },
    {
        .name = "Peas",
        .description = "4 in 1 deal, now with a secret ingredient: poison",
        .health = 20.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 1.4,
        .count = 4,
        .rarity = RarityID::kEpic,
        .attributes = {
            .clump_radius = 8,
            .secondary_reload = 0.1,
            .poison_damage = {
                .damage = 20.0,
                .time = 0.5
            },
            .defend_only = 1,
            .split_projectile = 1,
        }
    },
    {
        .name = "Egg",
        .description = "Something interesting might pop out of this",
        .health = 50.0,
        .damage = 1.0,
        .radius = 15.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = { 
            .secondary_reload = 1.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kBeetle
        }
    },
    {
        .name = "Rose",
        .description = "Extremely powerful rose, almost unheard of",
        .health = 5.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 3.5,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = { 
            .secondary_reload = 1.0,
            .burst_heal = 22,
            .defend_only = 1
        }
    },
    {
        .name = "Stick",
        .description = "Harnesses the power of the wind",
        .health = 10.0,
        .damage = 1.0,
        .radius = 15.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = { 
            .secondary_reload = 2.0,
            .defend_only = 1,
            .icon_angle = 1,
            .spawns = MobID::kSandstorm,
            .spawn_count = 2
        }
    },
    {
        .name = "Stinger",
        .description = "It really hurts, but it's really fragile",
        .health = 10.0,
        .damage = 35.0,
        .radius = 7.0,
        .reload = 4.0,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10
        }
    },
    {
        .name = "Web",
        .description = "It's really sticky",
        .health = 5.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 3.0,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10,
            .secondary_reload = 0.5,
            .defend_only = 1,
        }
    },
    {
        .name = "Antennae",
        .description = "Allows your flower to sense foes from farther away",
        .health = 0.0,
        .damage = 0.0,
        .radius = 12.5,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .vision_factor = 0.6,
            .equipment = EquipmentFlags::kAntennae 
        }
    },
    {
        .name = "Cactus",
        .description = "Not very strong, but somehow increases your maximum health",
        .health = 15.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10,
            .extra_health = 40,
        }
    },
    {
        .name = "Heaviest",
        .description = "This thing is so heavy that nothing gets in the way",
        .health = 200.0,
        .damage = 10.0,
        .radius = 12.0,
        .reload = 20.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {}
    },
    {
        .name = "Third Eye",
        .description = "Allows your flower to extend petals further out",
        .health = 0.0,
        .damage = 0.0,
        .radius = 20.0,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .extra_range = 75,
            .equipment = EquipmentFlags::kThirdEye
        }
    },

    
    {
        .name = "Cactus",
        .description = "Turns your flower poisonous. Enemies will take poison damage on contact",
        .health = 15.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .extra_health = 20,
            .poison_damage = {
                .damage = 1.0,
                .time = 6.0
            }
        }
    },
    {
        .name = "Salt",
        .description = "Reflects some damage dealt to the flower. Does not stack with itself",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kRare,
        .attributes = {
            .damage_reflection = 0.25
        }
    },
    {
        .name = "Basic",
        .description = "Something incredibly rare and useless",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {}
    },
    {
        .name = "Square",
        .description = "This shape... it looks familiar...",
        .health = 10.0,
        .damage = 10.0,
        .radius = 16.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            .icon_angle = M_PI / 4 + 0.25,
        }
    },
    
    {
        .name = "Lotus",
        .description = "Absorbs some poison damage taken by the flower",
        .health = 10.0,
        .damage = 5.0,
        .radius = 12.0,
        .reload = 2.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .poison_armor = 5,
            .icon_angle = 0.1
        }
    },
    {
        .name = "Cutter",
        .description = "Increases the flower's body damage",
        .health = 0.0,
        .damage = 0.0,
        .radius = 40.0,
        .reload = 0.0,
        .count = 0,
        .rarity = RarityID::kEpic,
        .attributes = { 
            .extra_body_damage = 15,
            .equipment = EquipmentFlags::kCutter
        }
    },
    {
        .name = "Yin Yang",
        .description = "Alters the flower's petal rotation in interesting ways",
        .health = 15.0,
        .damage = 15.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {}
    },
    {
        .name = "Yggdrasil",
        .description = "Unfortunately, its powers are useless here",
        .health = 10.0,
        .damage = 1.0,
        .radius = 12.0,
        .reload = 10.0,
        .count = 1,
        .rarity = RarityID::kUnique,
        .attributes = {
            .defend_only = 1,
            .icon_angle = M_PI
        }
    },
    {
        .name = "Rice",
        .description = "Spawns instantly, but not very strong",
        .health = 1.0,
        .damage = 5.0,
        .radius = 13.0,
        .reload = 0.04,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .icon_angle = 0.7
        }
    },
    {
        .name = "Bone",
        .description = "Sturdy",
        .health = 15.0,
        .damage = 12.0,
        .radius = 12.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .armor = 8,
            .icon_angle = 1
        }
    },
    {
        .name = "Yucca",
        .description = "Heals the flower, but only while in the defensive position",
        .health = 10.0,
        .damage = 5.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kUnusual,
        .attributes = {
            .constant_heal = 1.5,
            .icon_angle = -1
        }
    },
    {
        .name = "Corn", 
        .description = "Takes a long time to spawn, but has a lot of health",
        .health = 2000.0,
        .damage = 1.0,
        .radius = 16.0,
        .reload = 35.0,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .icon_angle = 0.5
        }
    },
    {
        .name = "Leaf",
        .description = "A very special leaf, seems to allow you to use your other petals even more.",
        .health = 10.0,
        .damage = 8.0,
        .radius = 10.0,
        .reload = 1.0,
        .count = 1,
        .rarity = RarityID::kLegendary,
                .attributes = {
            .reload_reduction = 0.21f,
            .icon_angle = -1
        }
    },
    {
        .name = "Peas",
        .description = "4 in 1 deal, now with a secret ingredient: poison",
        .health = 20.0,
        .damage = 10.0,
        .radius = 12.0,
        .reload = 1.4,
        .count = 4,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .clump_radius = 10,
            .secondary_reload = 0.1,
            .poison_damage = { .damage = 35.0, .time = 1.0 },
            .defend_only = 1,
            .split_projectile = 1,
            .icon_angle = 0.2
        }
    },
    {
        .name = "Soil",
        .description = "The bigger, the better!",
        .health = 10.0,
        .damage = 10.0,
        .radius = 10.0,
        .reload = 2.5,
        .count = 1,
        .rarity = RarityID::kEpic,
        .attributes = {
            .extra_flower_radius = 10.0f,
            .extra_health = 35.0f
        }
    },
    {
        .name = "Faster",
        .description = "So fast I can barely see it",
        .health = 5.0,
        .damage = 8.0,
        .radius = 7.0,
        .reload = 0.5,
        .count = 3,
        .rarity = RarityID::kLegendary,
        .attributes = {
            .extra_rotation_speed = 1.7f
        }
    }, 
}};

std::array<struct MobData, MobID::kNumMobs> const MOB_DATA = {{
    {
        .name = "Baby Ant",
        .description = "Weak and defenseless, but big dreams.",
        .rarity = RarityID::kCommon,
        .health = {10.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 1,
        .drops = {
            PetalID::kLight,
            PetalID::kLeaf,
            PetalID::kRice,
            PetalID::kTriplet,
            PetalID::kTwin
        },
        .drop_rates = {
            /* PetalID::kLight */ 44.00f,
            /* PetalID::kLeaf */ 26.00f,
            /* PetalID::kRice */ 0.50f,
            /* PetalID::kTriplet */ 0.06f,
            /* PetalID::kTwin */ 12.00f
        },
        .attributes = {}
    },
    {
        .name = "Worker Ant",
        .description = "It's temperamental, probably from working all the time.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 3,
        .drops = {
            PetalID::kLight,
            PetalID::kCorn,
            PetalID::kLeaf,
            PetalID::kTriplet,
            PetalID::kTwin
        },
        .drop_rates = {
            /* PetalID::kLight */ 46.00f,
            /* PetalID::kCorn */ 0.6f,
            /* PetalID::kLeaf */ 28.0f,
            /* PetalID::kTriplet */ 0.06f,
            /* PetalID::kTwin */ 13.0f
        },
        .attributes = {}
    },
    {
        .name = "Soldier Ant",
        .description = "It's got wings and it's ready to use them.",
        .rarity = RarityID::kUnusual,
        .health = {40.0},
        .damage = 10.0,
        .radius = {14.0},
        .xp = 5,
        .drops = {
            PetalID::kIris,
            PetalID::kFaster,
            PetalID::kTriplet,
            PetalID::kTwin,
            PetalID::kWing
        },
        .drop_rates = {
            /* PetalID::kIris */ 12.00f,
            /* PetalID::kFaster */ 3.00f,
            /* PetalID::kTriplet */ 0.04f,
            /* PetalID::kTwin */ 8.00f,
            /* PetalID::kWing */ 0.80f
        },
        .attributes = {}
    },
    {
        .name = "Bee",
        .description = "It stings. Don't touch it.",
        .rarity = RarityID::kCommon,
        .health = {15.0},
        .damage = 50.0,
        .radius = {20.0},
        .xp = 4,
        .drops = {
            PetalID::kLight,
            PetalID::kBubble,
            PetalID::kStinger,
            PetalID::kTriplet,
            PetalID::kTwin,
            PetalID::kWing
        },
        .drop_rates = {
            /* PetalID::kLight */ 12.00f,
            /* PetalID::kBubble */ 0.60f,
            /* PetalID::kStinger */ 7.00f,
            /* PetalID::kTriplet */ 0.01f,
            /* PetalID::kTwin */ 3.00f,
            /* PetalID::kWing */ 0.30f
        },
        .attributes = {}
    },
    {
        .name = "Ladybug",
        .description = "Cute and harmless.",
        .rarity = RarityID::kCommon,
        .health = {25.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 3,
        .drops = {
            PetalID::kLight,
            PetalID::kBubble,
            PetalID::kRose,
            PetalID::kTwin,
            PetalID::kWing
        },
        .drop_rates = {
            /* PetalID::kLight */ 7.00f,
            /* PetalID::kBubble */ 0.40f,
            /* PetalID::kRose */ 11.00f,
            /* PetalID::kTwin */ 2.00f,
            /* PetalID::kWing */ 0.20f
        },
        .attributes = {}
    },
    {
        .name = "Beetle",
        .description = "It's hungry and flowers are its favorite meal.",
        .rarity = RarityID::kUnusual,
        .health = {40.0},
        .damage = 35.0,
        .radius = {35.0},
        .xp = 10,
        .drops = {
            PetalID::kIris,
            PetalID::kSalt,
            PetalID::kTriplet,
            PetalID::kWing
        },
        .drop_rates = {
            /* PetalID::kIris */ 9.00f,
            /* PetalID::kSalt */ 6.00f,
            /* PetalID::kTriplet */ 0.03f,
            /* PetalID::kWing */ 0.60f
        },
        .attributes = {}
    },
    {
        .name = "Massive Ladybug",
        .description = "Much larger, but still cute.",
        .rarity = RarityID::kEpic,
        .health = {1000.0},
        .damage = 10.0,
        .radius = {90.0},
        .xp = 400,
        .drops = {
            PetalID::kBubble,
            PetalID::kDahlia,
            PetalID::kRose,
            PetalID::kAzalea
        },
        .drop_rates = {
            /* PetalID::kBubble */ 100.00f,
            /* PetalID::kDahlia */ 100.00f,
            /* PetalID::kRose */ 100.00f,
            /* PetalID::kAzalea */ 100.00f
        },
        .attributes = {}
    },
    {
        .name = "Massive Beetle",
        .description = "Someone overfed this one, you might be next.",
        .rarity = RarityID::kRare,
        .health = {300.0},
        .damage = 20.0,
        .radius = {75.0},
        .xp = 50,
        .drops = {
            PetalID::kAntEgg,
            PetalID::kIris,
            PetalID::kTriplet,
            PetalID::kWing,
            PetalID::kBeetleEgg
        },
        .drop_rates = {
            /* PetalID::kAntEgg */ 0.60f,
            /* PetalID::kIris */ 100.00f,
            /* PetalID::kTriplet */ 1.00f,
            /* PetalID::kWing */ 31.00f,
            /* PetalID::kBeetleEgg */ 0.05f
        },
        .attributes = { 
            .aggro_radius = 750
        }
    },
    {
        .name = "Ladybug",
        .description = "Cute and harmless... if left unprovoked.",
        .rarity = RarityID::kUnusual,
        .health = {25.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 5,
        .drops = {
            PetalID::kDahlia,
            PetalID::kBubble,
            PetalID::kWing,
            PetalID::kYinYang,
            PetalID::kAzalea,
            PetalID::kTriplet
        },
        .drop_rates = {
            /* PetalID::kDahlia */ 39.00f,
            /* PetalID::kBubble */ 8.00f,
            /* PetalID::kWing */ 4.00f,
            /* PetalID::kYinYang */ 2.00f,
            /* PetalID::kAzalea */ 0.20f,
            /* PetalID::kTriplet */ 0.20f
        },
        .attributes = {}
    },
    {
        .name = "Hornet",
        .description = "These aren't quite as nice as the little bees.",
        .rarity = RarityID::kUnusual,
        .health = {40.0},
        .damage = 40.0,
        .radius = {40.0},
        .xp = 12,
        .drops = {
            PetalID::kDandelion,
            PetalID::kAntennae,
            PetalID::kMissile,
            PetalID::kWing
        },
        .drop_rates = {
            /* PetalID::kDandelion */ 14.00f,
            /* PetalID::kAntennae */ 0.03f,
            /* PetalID::kMissile */ 9.00f,
            /* PetalID::kWing */ 3.00f
        },
        .attributes = {
            .aggro_radius = 600
        }
    },
    {
        .name = "Cactus",
        .description = "This one's prickly, don't touch it either.",
        .rarity = RarityID::kCommon,
        .health = {28.0, 42.0},
        .damage = 35.0,
        .radius = {30.0, 60.0},
        .xp = 2,
        .drops = {
            PetalID::kCactus,
            PetalID::kMissile,
            PetalID::kPoisonCactus,
            PetalID::kStinger,
            PetalID::kTricac
        },
        .drop_rates = {
            /* PetalID::kCactus */ 3.00f,
            /* PetalID::kMissile */ 0.90f,
            /* PetalID::kPoisonCactus */ 0.10f,
            /* PetalID::kStinger */ 5.00f,
            /* PetalID::kTricac */ 0.005f
        },
        .attributes = {
            .stationary = 1
        }
    },
    {
        .name = "Rock",
        .description = "A rock. It doesn't do much.",
        .rarity = RarityID::kCommon,
        .health = {8.0, 16.0},
        .damage = 10.0,
        .radius = {25.0, 30.0},
        .xp = 1,
        .drops = {
            PetalID::kHeavy,
            PetalID::kRock
        },
        .drop_rates = {
            /* PetalID::kHeavy */ 10.00f,
            /* PetalID::kRock */ 0.80f
        },
        .attributes = {
            .stationary = 1
        }
    },
    {
        .name = "Boulder",
        .description = "A bigger rock. It also doesn't do much.",
        .rarity = RarityID::kUnusual,
        .health = {48.0, 80.0},
        .damage = 10.0,
        .radius = {50.0, 75.0},
        .xp = 10,
        .drops = {
            PetalID::kHeavy,
            PetalID::kRock,
            PetalID::kHeaviest
        },
        .drop_rates = {
            /* PetalID::kHeavy */ 83.00f,
            /* PetalID::kRock */ 4.00f,
            /* PetalID::kHeaviest */ 0.30f
        }, 
        .attributes = {
            .stationary = 1
        }
    },
    {
        .name = "Centipede",
        .description = "It's just there doing its thing.",
        .rarity = RarityID::kUnusual,
        .health = {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 2,
        .drops = {
            PetalID::kLight,
            PetalID::kTwin,
            PetalID::kLeaf,
            PetalID::kPeas,
            PetalID::kTriplet,
            PetalID::kGoldenLeaf
        },
        .drop_rates = {
            /* PetalID::kLight */ 9.00f,
            /* PetalID::kTwin */ 2.00f,
            /* PetalID::kLeaf */ 5.00f,
            /* PetalID::kPeas */ 3.00f,
            /* PetalID::kTriplet */ 0.01f,
            /* PetalID::kGoldenLeaf */ 0.01f
        },
        .attributes = {
            .segments = 10
        }
    },
    {
        .name = "Evil Centipede",
        .description = "This one loves flowers.",
        .rarity = RarityID::kRare,
        .health = {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 3,
        .drops = {
            PetalID::kIris,
            PetalID::kPoisonPeas,
            PetalID::kTriplet,
            PetalID::kLPeas,
        },
        .drop_rates = {
            /* PetalID::kIris */ 82.00f,
            /* PetalID::kPoisonPeas */ 2.00f,
            /* PetalID::kTriplet */ 0.03f,
            /* PetalID::kLPeas */ 0.01f,
        },
        .attributes = { 
            .segments = 10, 
            .poison_damage = {
                .damage = 5.0,
                .time = 2.0
            }
        }
    },
    {
        .name = "Desert Centipede",
        .description = "It doesn't like it when you interrupt its run.",
        .rarity = RarityID::kRare,
        .health= {50.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 4,
                .drops = {
            PetalID::kSand,
            PetalID::kFaster,
            PetalID::kSalt,
            PetalID::kTriFaster,
        },
                .drop_rates = {
            /* PetalID::kSand */ 12.00f,
            /* PetalID::kFaster */ 4.00f,
            /* PetalID::kSalt */ 8.00f,
            /* PetalID::kTriFaster */ 0.03f,
        },
        .attributes = {
            .segments = 6
        }
    },
    {
        .name = "Sandstorm",
        .description = "Quite unpredictable.",
        .rarity = RarityID::kUnusual,
        .health = {30.0, 45.0},
        .damage = 40.0,
        .radius = {32.0, 48.0},
        .xp = 5,
        .drops = {
            PetalID::kSand,
            PetalID::kStick
        },
        .drop_rates = {
            /* PetalID::kSand */ 14.00f,
            /* PetalID::kStick */ 0.05f
        },
        .attributes = {}
    },
    {
        .name = "Scorpion",
        .description = "This one stings, now with poison.",
        .rarity = RarityID::kUnusual,
        .health = {35.0},
        .damage = 10.0,
        .radius = {35.0},
        .xp = 10,
        .drops = {
            PetalID::kIris,
            PetalID::kPincer,
            PetalID::kTriplet,
            PetalID::kLotus
        }, 
        .drop_rates = {
            /* PetalID::kIris */ 12.00f,
            /* PetalID::kPincer */ 9.00f,
            /* PetalID::kTriplet */ 0.01f,
            /* PetalID::kLotus */ 0.20f
        },  
        .attributes = {
            .poison_damage = {
                .damage = 10.0,
                .time = 1.0
            }
        }
    },
    {
        .name = "Spider",
        .description = "Spooky.",
        .rarity = RarityID::kUnusual,
        .health = {20.0},
        .damage = 25.0,
        .radius = {15.0},
        .xp = 8,
        .drops = {
            PetalID::kStinger,
            PetalID::kIris,
            PetalID::kWeb,
            PetalID::kFaster,
            PetalID::kTriweb
        },
        .drop_rates = {
            /* PetalID::kStinger */ 18.00f,
            /* PetalID::kIris */ 12.00f,
            /* PetalID::kWeb */ 9.00f,
            /* PetalID::kFaster */ 3.00f,
            /* PetalID::kTriweb */ 0.02f
        },
        .attributes = { 
            .poison_damage = {
                .damage = 5.0,
                .time = 3.0
            }
        }
    },
    {
        .name = "Ant Hole",
        .description = "Ants go in, and come out. Can't explain that.",
        .rarity = RarityID::kRare,
        .health = {300.0},
        .damage = 15.0,
        .radius = {45.0},
        .xp = 25,
                .drops = {
            PetalID::kIris,
            PetalID::kWing,
            PetalID::kAntEgg,
            PetalID::kSoil,
        },
                .drop_rates = {
            /* PetalID::kIris */ 100.00f,
            /* PetalID::kWing */ 31.00f,
            /* PetalID::kAntEgg */ 6.00f,
            /* PetalID::kSoil */ 5.00f,
        },
        .attributes = {
            .stationary = 1 
        }
    },
    {
        .name = "Queen Ant",
        .description = "You must have done something really bad if she's chasing you.",
        .rarity = RarityID::kRare,
        .health = {200.0},
        .damage = 20.0,
        .radius = {25.0},
        .xp = 15,
        .drops = {
            PetalID::kTwin,
            PetalID::kFaster,
            PetalID::kWing,
            PetalID::kAntEgg,
            PetalID::kTriplet,
            PetalID::kTringer
        },
        .drop_rates = {
            /* PetalID::kTwin */ 100.00f,
            /* PetalID::kFaster */ 100.00f,
            /* PetalID::kWing */ 31.00f,
            /* PetalID::kAntEgg */ 6.00f,
            /* PetalID::kTriplet */ 1.00f,
            /* PetalID::kTringer */ 0.60f
        },
        .attributes = {
            .aggro_radius = 750
        }
    },
    {
        .name = "Ladybug",
        .description = "This one is shiny... I wonder what it could mean...",
        .rarity = RarityID::kEpic,
        .health = {25.0},
        .damage = 10.0,
        .radius = {30.0},
        .xp = 30,
        .drops = {
            PetalID::kRose,
            PetalID::kTwin,
            PetalID::kDahlia,
            PetalID::kWing,
            PetalID::kBubble,
            PetalID::kYggdrasil
        },
        .drop_rates = {
            /* PetalID::kRose */ 100.00f,
            /* PetalID::kTwin */ 39.00f,
            /* PetalID::kDahlia */ 34.00f,
            /* PetalID::kWing */ 4.00f,
            /* PetalID::kBubble */ 8.00f,
            /* PetalID::kYggdrasil */ 0.40f
        },
        .attributes = {}
    },
    {
        .name = "Square",
        .description = "This shape... It looks familiar...",
        .rarity = RarityID::kUnique,
        .health = {15.0},
        .damage = 10.0,
        .radius = {40.0},
        .xp = 1,
        .drops = {
            PetalID::kSquare
        },
        .drop_rates = {
            /* PetalID::kSquare */ 100.00f
        },
        .attributes = {
            .stationary = 1
        }
    },
    {
        .name = "Digger",
        .description = "Friend or foe? You'll never know...",
        .rarity = RarityID::kEpic,
        .health = {250.0},
        .damage = 25.0,
        .radius = {40.0},
        .xp = 1,
        .drops = {
            PetalID::kCutter
        },
        .drop_rates = {
            /* PetalID::kCutter */ 39.00f
        },
        .attributes = {}
    },
}};



uint32_t score_to_pass_level(uint32_t level) {
    return (uint32_t)(pow(1.06, level - 1) * level) + 3;
}

uint32_t score_to_level(uint32_t score) {
    uint32_t level = 1;
    while (level < MAX_LEVEL) {
        uint32_t level_score = score_to_pass_level(level);
        if (score < level_score) break;
        score -= level_score;
        ++level;
    }
    return level;
}

uint32_t level_to_score(uint32_t level) {
    uint32_t score = 0;
    for (uint32_t i = 1; i < level; ++i)
        score += score_to_pass_level(i);
    return score;
}

uint32_t loadout_slots_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    uint32_t ret = 5 + level / LEVELS_PER_EXTRA_SLOT;
    if (ret > MAX_SLOT_COUNT) return MAX_SLOT_COUNT;
    return ret;
}

float hp_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return BASE_HEALTH + level;
}