#include <Server/Bots/Priorities.hh>

#include <cmath>
#include <vector>
#include <unordered_map>
#include <algorithm>  // for sort

namespace Bots { namespace Priority {

static inline bool in_fov(Context const &c, Entity const &e) {
    return std::fabs(e.get_x() - c.cx) <= c.half_w && std::fabs(e.get_y() - c.cy) <= c.half_h;
}

static inline uint8_t rarity_of(PetalID::T id) {
    return (id == PetalID::kNone) ? 0 : PETAL_DATA[id].rarity;
}

static inline float damage_of(PetalID::T id) {
    return (id == PetalID::kNone) ? 0.0f : PETAL_DATA[id].damage + PETAL_DATA[id].attributes.extra_body_damage;
}

static inline bool is_ranged(PetalID::T id) {
    if (id == PetalID::kNone) return false;
    switch(id) {
        case PetalID::kDandelion:
        case PetalID::kPeas:
        case PetalID::kMissile:
        case PetalID::kPoisonPeas:
        case PetalID::kLPeas:
        case PetalID::kTriFaster:
        case PetalID::kFaster:
        case PetalID::kTriplet:
            return true;
        default:
            return PETAL_DATA[id].attributes.split_projectile || PETAL_DATA[id].attributes.spawn_count > 0;
    }
}

static inline bool has_basic(Entity const &player) {
    // Check both main and secondary
    uint8_t N = player.get_loadout_count();
    for (uint8_t i = 0; i < N + MAX_SLOT_COUNT; ++i)
        if (player.get_loadout_ids(i) == PetalID::kBasic) return true;
    return false;
}

// Strictly check Basic in MAIN only
static inline bool has_basic_in_main(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i)
        if (player.get_loadout_ids(i) == PetalID::kBasic) return true;
    return false;
}

// Convenience
static inline bool is_rose(PetalID::T id) { return id == PetalID::kRose; }
static inline float rose_damage_threshold() {
    float d = damage_of(PetalID::kRose);
    return (d > 0.0f ? d : 5.0f); // default to 5 if data not populated
}

// --- PURE HEAL definition (broadened) ---
static inline bool is_pure_heal(PetalID::T id) {
    if (id == PetalID::kNone) return false;
    auto const &pd = PETAL_DATA[id];
    bool heals = (pd.attributes.burst_heal > 0 || pd.attributes.constant_heal > 0);
    float dmg = damage_of(id);
    if (!heals) return false;

    if (is_rose(id)) return true;   // explicitly pure-heal behaviorally
    return (dmg < 0.5f);            // tolerate tiny non-zero damage
}

static inline bool has_heal_in_main_excl_leaf(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_pure_heal(pid)) return true;
    }
    return false;
}

// NEW: check for any pure-heal in MAIN (Leaf ignored)
static inline bool main_has_pure_heal(Entity const &player) {
    uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_pure_heal(pid)) return true;
    }
    return false;
}

static inline int count_pure_heals_in_main(Entity const &player) {
    int cnt = 0; uint8_t N = player.get_loadout_count();
    for (uint8_t i=0;i<N;++i) {
        PetalID::T pid = player.get_loadout_ids(i);
        if (pid == PetalID::kLeaf) continue;
        if (is_pure_heal(pid)) ++cnt;
    }
    return cnt;
}

struct HealPhaseState { bool was_low = false; bool cleanup_done = false; };
static std::unordered_map<uint16_t, HealPhaseState> g_heal_state;

// Helper: pick the best secondary slot whose damage strictly exceeds a threshold
static int pick_best_secondary_damage_slot(Entity const &player, uint8_t secStart, float threshold) {
    int best_j = -1; float best_dmg = -1.0f;
    for (uint8_t j = 0; j < MAX_SLOT_COUNT; ++j) {
        PetalID::T sid = player.get_loadout_ids(secStart + j);
        if (sid == PetalID::kNone) continue;
        if (!is_damage_petal(sid)) continue;
        float dmg = damage_of(sid);
        if (dmg > threshold && dmg > best_dmg) { best_dmg = dmg; best_j = (int)j; }
    }
    return best_j;
}

// Fallback: any "reasonable" swap-in (damage/Leaf/Basic) when nothing beats threshold
static int pick_any_reasonable_swapin(Entity const &player, uint8_t secStart) {
    for (uint8_t j=0; j<MAX_SLOT_COUNT; ++j) {
        PetalID::T sid = player.get_loadout_ids(secStart + j);
        if (sid == PetalID::kNone) continue;
        if (is_damage_petal(sid) || sid == PetalID::kLeaf || sid == PetalID::kBasic) return (int)j;
    }
    return -1;
}

// --- XP trashing helpers ---
static inline bool is_protected_secondary(PetalID::T sid, uint8_t worst_main_rarity) {
    if (sid == PetalID::kNone) return true; // nothing to do
    if (sid == PetalID::kLeaf) return true; // keep utility
    if (is_pure_heal(sid)) return true;     // heals (Rose cleanup handles them)
    if (rarity_of(sid) > worst_main_rarity) return true; // could upgrade main later
    return false;
}

// compute reserved swap-ins from secondary for future heal removal.
// Reserve up to 'needed' best DAMAGE petals (by rarity, then damage).
static void compute_reserved_swapins(Entity const &player, uint8_t secStart, int needed, bool reserved[MAX_SLOT_COUNT]) {
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) reserved[j] = false;
    if (needed <= 0) return;

    struct Cand { int j; uint8_t r; float dmg; };
    std::vector<Cand> cands; cands.reserve(MAX_SLOT_COUNT);

    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        PetalID::T sid = player.get_loadout_ids(secStart + j);
        if (sid == PetalID::kNone) continue;
        if (!is_damage_petal(sid)) continue;       // only damage petals are useful to replace heals
        cands.push_back({ (int)j, rarity_of(sid), damage_of(sid) });
    }

    if (cands.empty()) return;

    std::sort(cands.begin(), cands.end(), [](Cand const &a, Cand const &b){
        if (a.r != b.r) return a.r > b.r;          // higher rarity first
        if (a.dmg != b.dmg) return a.dmg > b.dmg;  // then higher damage
        return a.j < b.j;
    });

    int keep = std::min<int>(needed, (int)cands.size());
    for (int k=0;k<keep;++k) reserved[cands[k].j] = true;
}

static int pick_lowest_rarity_trash_slot(Entity const &player, uint8_t secStart, uint8_t worst_main_rarity) {
    int best_j = -1; uint8_t best_r = 255; float best_dmg = 1e9f;
    for (uint8_t j=0; j<MAX_SLOT_COUNT; ++j) {
        uint8_t idx = secStart + j;
        PetalID::T sid = player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        if (is_protected_secondary(sid, worst_main_rarity)) continue;
        uint8_t r = rarity_of(sid);
        float dmg = damage_of(sid);
        if (r < best_r || (r == best_r && dmg < best_dmg)) {
            best_r = r; best_dmg = dmg; best_j = (int)j;
        }
    }
    return best_j;
}

// trash picker that *also* respects a reserved mask
static int pick_lowest_rarity_trash_slot_reserve(Entity const &player, uint8_t secStart, uint8_t worst_main_rarity, bool const reserved[MAX_SLOT_COUNT]) {
    int best_j = -1; uint8_t best_r = 255; float best_dmg = 1e9f;
    for (uint8_t j=0; j<MAX_SLOT_COUNT; ++j) {
        if (reserved[j]) continue; // do not trash reserved future swap-ins
        uint8_t idx = secStart + j;
        PetalID::T sid = player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        if (is_protected_secondary(sid, worst_main_rarity)) continue;
        uint8_t r = rarity_of(sid);
        float dmg = damage_of(sid);
        if (r < best_r || (r == best_r && dmg < best_dmg)) {
            best_r = r; best_dmg = dmg; best_j = (int)j;
        }
    }
    return best_j;
}

Decision evaluate(Context const &ctx) {
    Decision d; // defaults to None, score 0

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

    // Priority 4: Low HP healing
    if (hpRatio < LOW_HP) {
        // 4a) If a healing petal drop is visible, go loot it (score 4)
        EntityID healDrop = NULL_ENTITY; float bestD2H = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_heal_only(pid)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (healDrop.null() || d2 < bestD2H) { healDrop = e.id; bestD2H = d2; }
        });
        if (!healDrop.null()) { d.type = Decision::Loot; d.target = healDrop; d.score = 4.0f; return d; }

        // 4b) Otherwise, seek mobs likely to drop heals (score 4)
        EntityID healMob = NULL_ENTITY; float bestD2M = 0.0f; uint8_t bestMinRarity = 0;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 150, ctx.half_h + 150, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (!in_fov(ctx, e)) return;
            auto const &md = MOB_DATA[e.get_mob_id()];
            uint8_t minHealRarity = 255; bool dropsHeal = false;
            for (uint32_t i=0;i<MAX_DROPS_PER_MOB;++i) {
                PetalID::T pid = md.drops[i]; if (pid >= PetalID::kNumPetals) continue;
                if (is_heal_only(pid)) { dropsHeal = true; minHealRarity = std::min<uint8_t>(minHealRarity, rarity_of(pid)); }
            }
            if (!dropsHeal) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (healMob.null() || d2 < bestD2M || (std::fabs(d2 - bestD2M) < 1e-3 && minHealRarity > bestMinRarity)) {
                healMob = e.id; bestD2M = d2; bestMinRarity = minHealRarity;
            }
        });
        if (!healMob.null()) { d.type = Decision::SeekHealMob; d.target = healMob; d.score = 4.0f; return d; }
    }

    // FULL HP and heal in main (excluding Leaf): pursue any visible damage drop (prefer dmg > Rose)
    if (hpRatio >= FULL_HP && has_heal_in_main_excl_leaf(ctx.player)) {
        float thr = rose_damage_threshold();
        EntityID strongDrop = NULL_ENTITY; float strongD2 = 0.0f;
        EntityID anyDmgDrop = NULL_ENTITY; float anyD2 = 0.0f;

        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_damage_petal(pid)) return;

            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;

            if (damage_of(pid) > thr) {
                if (strongDrop.null() || d2 < strongD2) { strongDrop = e.id; strongD2 = d2; }
            }
            if (anyDmgDrop.null() || d2 < anyD2) { anyDmgDrop = e.id; anyD2 = d2; }
        });

        if (!strongDrop.null()) { d.type = Decision::Loot; d.target = strongDrop; d.score = 4.0f; return d; }
        if (!anyDmgDrop.null()) { d.type = Decision::Loot; d.target = anyDmgDrop; d.score = 4.0f; return d; }
    }

    // Priority 3: Rarity upgrade over worst main
    uint8_t N = ctx.player.get_loadout_count();
    uint8_t worst = 255; for (uint8_t i=0;i<N;++i) worst = std::min<uint8_t>(worst, rarity_of(ctx.player.get_loadout_ids(i)));
    EntityID bestDrop = NULL_ENTITY; uint8_t bestR = 0; float bestD2 = 0.0f;
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return;
        if (!e.has_component(kDrop)) return;
        if (!in_fov(ctx, e)) return;
        PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
        uint8_t r = rarity_of(pid);
        if (r > worst) {
            float dx = e.get_x() - ctx.player.get_x();
            float dy = e.get_y() - ctx.player.get_y();
            float d2 = dx*dx + dy*dy;
            if (bestDrop.null() || r > bestR || (r == bestR && d2 < bestD2)) { bestDrop = e.id; bestR = r; bestD2 = d2; }
        }
    });
    if (!bestDrop.null()) { d.type = Decision::Loot; d.target = bestDrop; d.score = 3.0f; return d; }

    // Priority 2: Replace Basics with any damage
    if (has_basic(ctx.player)) {
        EntityID dmgDrop = NULL_ENTITY; float bestD2b = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return; if (!e.has_component(kDrop)) return; if (!in_fov(ctx, e)) return;
            PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return; if (!is_damage_petal(pid)) return;
            float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
            if (dmgDrop.null() || d2 < bestD2b) { dmgDrop = e.id; bestD2b = d2; }
        });
        if (!dmgDrop.null()) { d.type = Decision::Loot; d.target = dmgDrop; d.score = 2.0f; return d; }
    }

    // Priority 1.5: XP trashing — loot low-rarity petals we would never use (to trash)
    // Skip XP mode entirely while Basics remain in MAIN.
    if (!has_basic_in_main(ctx.player)) {
        uint8_t Nloc = ctx.player.get_loadout_count();
        uint8_t worst_main = 255;
        for (uint8_t i=0;i<Nloc;++i) worst_main = std::min<uint8_t>(worst_main, rarity_of(ctx.player.get_loadout_ids(i)));

        const bool currently_would_swap_heals = has_heal_in_main_excl_leaf(ctx.player);

        EntityID trashDrop = NULL_ENTITY; float trashD2 = 0.0f;
        ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return;
            if (!e.has_component(kDrop)) return;
            if (!in_fov(ctx, e)) return;

            PetalID::T pid = e.get_drop_id();
            if (pid == PetalID::kNone) return;
            if (is_pure_heal(pid)) return;       // never farm heals for trashing
            if (pid == PetalID::kLeaf) return;   // keep utility petals off the trash route

            uint8_t r = rarity_of(pid);
            // Skip anything we might want later (upgrade)…
            if (r > worst_main) return;
            // …or that we'd use to replace heals ONLY if we actually have a heal in main right now.
            if (currently_would_swap_heals && is_damage_petal(pid) && damage_of(pid) > rose_damage_threshold()) return;

            float dx = e.get_x() - ctx.player.get_x();
            float dy = e.get_y() - ctx.player.get_y();
            float d2 = dx*dx + dy*dy;
            if (trashDrop.null() || d2 < trashD2) { trashDrop = e.id; trashD2 = d2; }
        });

        if (!trashDrop.null()) { d.type = Decision::Loot; d.target = trashDrop; d.score = 1.5f; return d; }
    }

    // Rule 1: If nothing else, attack nearest visible mob
    EntityID closest = NULL_ENTITY; float bestDist2 = 0.0f;
    ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
        if (!sm->ent_alive(e.id)) return; if (!e.has_component(kMob)) return; if (e.get_team() == ctx.player.get_team()) return; if (!in_fov(ctx, e)) return;
        float dx = e.get_x() - ctx.player.get_x(); float dy = e.get_y() - ctx.player.get_y(); float d2 = dx*dx + dy*dy;
        if (closest.null() || d2 < bestDist2) { closest = e.id; bestDist2 = d2; }
    });
    if (!closest.null()) { d.type = Decision::Attack; d.target = closest; d.score = 1.0f; return d; }

    // Priority 0.5: Wander
    d.type = Decision::Wander; d.score = 0.5f; return d;
}

void apply_rearrange(Context &ctx) {
    uint8_t mainN = ctx.player.get_loadout_count();
    uint8_t secStart = mainN;

    auto r_of = [&](PetalID::T id){ return rarity_of(id); };

    float hpRatio = ctx.player.health / std::max(1.0f, ctx.player.max_health);

    // Track heal phase transitions for one-time cleanup behavior
    HealPhaseState &hs = g_heal_state[ctx.player.id.id];
    if (hpRatio < LOW_HP) { hs.was_low = true; hs.cleanup_done = false; }

    // ============================
    // BASIC PURGE (runs before any XP trashing)
    // ============================
    for (uint8_t i=0;i<mainN;++i) {
        if (ctx.player.get_loadout_ids(i) != PetalID::kBasic) continue;

        // pick best secondary replacement: prefer damage; else Leaf; never pure-heal; never None; never Basic.
        int best_j = -1; float best_dmg = -0.1f; bool best_is_leaf = false;
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            uint8_t idx = secStart + j;
            PetalID::T sid = ctx.player.get_loadout_ids(idx);
            if (sid == PetalID::kNone) continue;
            if (sid == PetalID::kBasic) continue;
            if (is_pure_heal(sid)) continue;
            bool leaf = (sid == PetalID::kLeaf);
            float dmg = is_damage_petal(sid) ? damage_of(sid) : 0.0f;

            bool better =
                (dmg > best_dmg) ||
                (!leaf && best_is_leaf && dmg == best_dmg) ||
                (best_dmg < 0.0f && leaf);
            if (better) { best_j = (int)j; best_dmg = dmg; best_is_leaf = leaf; }
        }

        if (best_j >= 0) {
            uint8_t sidx = secStart + (uint8_t)best_j;
            PetalID::T repl = ctx.player.get_loadout_ids(sidx); // non–pure-heal
            ctx.player.set_loadout_ids((uint8_t)i, repl);
            ctx.player.set_loadout_ids(sidx, PetalID::kNone);   // trash the Basic we displaced
            return; // one change per tick
        }
    }

    // ============================
    // Priority 4 (LOW HP): Probabilistic MULTI-EQUIP of pure-heals from secondary
    // ============================
    if (hpRatio < LOW_HP) {
        // Collect pure-heals in secondary (Rose etc.)
        std::vector<uint8_t> heals;
        heals.reserve(MAX_SLOT_COUNT);
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            uint8_t idx = secStart + j;
            PetalID::T sid = ctx.player.get_loadout_ids(idx);
            if (is_pure_heal(sid)) heals.push_back(idx);
        }

        if (!heals.empty()) {
            auto pick_replacement_slot = [&](void)->int {
                // Choose replacement slot in main: lowest rarity; tie → prefer ranged; tie → lowest damage.
                // Also: avoid replacing existing pure-heal in main (no-op).
                int rep_i = -1; uint8_t rep_r = 255; float rep_dmg = 1e9f; bool rep_is_ranged = false;
                for (uint8_t i=0;i<mainN;++i) {
                    PetalID::T mid = ctx.player.get_loadout_ids(i);
                    if (is_pure_heal(mid)) continue; // keep existing pure-heals
                    uint8_t r = r_of(mid);
                    float dmg = damage_of(mid);
                    bool rng = is_ranged(mid);
                    if (r < rep_r ||
                       (r == rep_r && (rng && !rep_is_ranged)) ||
                       (r == rep_r && rng == rep_is_ranged && dmg < rep_dmg)) {
                        rep_i = (int)i; rep_r = r; rep_dmg = dmg; rep_is_ranged = rng;
                    }
                }
                return rep_i;
            };

            auto equip_one = [&](uint8_t heal_idx)->bool {
                int rep_i = pick_replacement_slot();
                if (rep_i < 0) return false; // no non-heal slot left to replace
                PetalID::T heal = ctx.player.get_loadout_ids(heal_idx);
                PetalID::T replaced = ctx.player.get_loadout_ids((uint8_t)rep_i);
                ctx.player.set_loadout_ids((uint8_t)rep_i, heal);
                ctx.player.set_loadout_ids(heal_idx, replaced);
                return true;
            };

            // Probabilities per order
            for (size_t k=0; k<heals.size(); ++k) {
                float chance = 0.05f;               // default for 5th+
                if (k == 0) chance = 1.00f;         // 1st heal
                else if (k == 1) chance = 0.50f;    // 2nd
                else if (k == 2) chance = 0.25f;    // 3rd
                else if (k == 3) chance = 0.10f;    // 4th

                if (frand() <= chance) {
                    equip_one(heals[k]);
                }
            }
            // Do not 'return' here—let other low-HP logic happen in future ticks.
        } else {
            // Fallback: legacy single-heal equip if only generic "heals-only" exist
            int heal_j = -1;
            for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
                PetalID::T sid = ctx.player.get_loadout_ids(secStart + j);
                if (is_heal_only(sid)) { heal_j = (int)j; break; }
            }
            if (heal_j >= 0) {
                // Choose replacement slot in main
                uint8_t rep_i = 0; uint8_t rep_r = 255; float rep_dmg = 1e9f; bool rep_is_ranged = false;
                for (uint8_t i=0;i<mainN;++i) {
                    PetalID::T mid = ctx.player.get_loadout_ids(i);
                    if (is_pure_heal(mid)) continue; // don't replace existing pure-heal
                    uint8_t r = r_of(mid);
                    float dmg = damage_of(mid);
                    bool rng = is_ranged(mid);
                    if (r < rep_r || (r == rep_r && (rng && !rep_is_ranged)) ||
                        (r == rep_r && rng == rep_is_ranged && dmg < rep_dmg)) {
                        rep_i = i; rep_r = r; rep_dmg = dmg; rep_is_ranged = rng;
                    }
                }
                uint8_t idx = secStart + (uint8_t)heal_j;
                PetalID::T replaced = ctx.player.get_loadout_ids(rep_i);
                PetalID::T heal = ctx.player.get_loadout_ids(idx);
                ctx.player.set_loadout_ids(rep_i, heal);
                ctx.player.set_loadout_ids(idx, replaced);
            }
        }
    }

    // ============================
    // Priority 4 (FULL HP): move pure-heals out of main
    // ============================
    if (hpRatio >= FULL_HP) {
        for (uint8_t i=0;i<mainN;++i) {
            PetalID::T mid = ctx.player.get_loadout_ids(i);
            if (mid == PetalID::kLeaf) continue;
            if (!is_pure_heal(mid)) continue;

            // Determine threshold: Rose => ~5; other heals => their own damage (often ~0)
            float thr = is_rose(mid) ? rose_damage_threshold() : damage_of(mid);

            // 1) Try to find a secondary petal strictly stronger than threshold
            int swap_j = pick_best_secondary_damage_slot(ctx.player, secStart, thr);

            // 2) Fallback: any reasonable damage/Leaf/Basic to ensure de-healing main
            if (swap_j < 0) swap_j = pick_any_reasonable_swapin(ctx.player, secStart);

            if (swap_j >= 0) {
                uint8_t idx = secStart + (uint8_t)swap_j;
                PetalID::T repl = ctx.player.get_loadout_ids(idx);
                ctx.player.set_loadout_ids(i, repl); // bring damage in
                ctx.player.set_loadout_ids(idx, mid); // move heal to secondary
            }
        }

        // One-time cleanup of excess pure-heal petals in secondary per heal cycle
        if (hs.was_low && !hs.cleanup_done && !main_has_pure_heal(ctx.player)) {
            std::vector<uint8_t> heals;
            heals.reserve(MAX_SLOT_COUNT);
            for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
                uint8_t idx = secStart + j;
                PetalID::T sid = ctx.player.get_loadout_ids(idx);
                if (is_pure_heal(sid)) heals.push_back(idx);
            }
            for (size_t k=0;k<heals.size();++k) {
                float chance = 0.0f;
                if (k == 0) chance = 0.02f;           // 1st heal: 2%
                else if (k == 1) chance = 0.25f;      // 2nd: 25%
                else if (k == 2) chance = 0.60f;      // 3rd: 60%
                else if (k == 3) chance = 0.85f;      // 4th: 85%
                else chance = 0.98f;                  // 5th+: 98%
                if (frand() < chance) {
                    ctx.player.set_loadout_ids(heals[k], PetalID::kNone);
                }
            }
            hs.cleanup_done = true;
            hs.was_low = false;
        }
    }

    // ============================
    // XP MODE (no Basics in MAIN):
    // trash unneeded low-rarity petals in secondary,
    // free space if a desirable ground drop exists,
    // and **reserve** enough damage petals to swap heals out later.
    // ============================
    if (!has_basic_in_main(ctx.player)) {
        // Compute worst main rarity once
        uint8_t worst_main = 255;
        for (uint8_t i=0;i<mainN;++i) worst_main = std::min<uint8_t>(worst_main, rarity_of(ctx.player.get_loadout_ids(i)));

        const bool currently_would_swap_heals = has_heal_in_main_excl_leaf(ctx.player);

        // Build reserved mask for future heal removal
        bool reserved[MAX_SLOT_COUNT]{};   // false init
        if (currently_would_swap_heals) {
            int heals_in_main = count_pure_heals_in_main(ctx.player);
            compute_reserved_swapins(ctx.player, secStart, heals_in_main, reserved);
        }

        // Determine if secondary is full
        bool secondary_full = true;
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            if (ctx.player.get_loadout_ids(secStart + j) == PetalID::kNone) { secondary_full = false; break; }
        }

        // Decide if there is any "desirable" ground drop in view
        bool want_ground = false;
        // a) low HP → any heal-only drop
        if (!want_ground && hpRatio < LOW_HP) {
            ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
                if (!sm->ent_alive(e.id)) return;
                if (!in_fov(ctx, e) || !e.has_component(kDrop)) return;
                PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
                if (is_heal_only(pid)) want_ground = true;
            });
        }
        // b) FULL HP w/ heal in main → any damage drop
        if (!want_ground && hpRatio >= FULL_HP && has_heal_in_main_excl_leaf(ctx.player)) {
            ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
                if (!sm->ent_alive(e.id)) return;
                if (!in_fov(ctx, e) || !e.has_component(kDrop)) return;
                PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
                if (is_damage_petal(pid)) want_ground = true;
            });
        }
        // c) Rarity upgrade present
        if (!want_ground) {
            ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
                if (!sm->ent_alive(e.id)) return;
                if (!in_fov(ctx, e) || !e.has_component(kDrop)) return;
                PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
                if (rarity_of(pid) > worst_main) want_ground = true;
            });
        }
        // d) Basic replacement present
        if (!want_ground && has_basic(ctx.player)) {
            ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
                if (!sm->ent_alive(e.id)) return;
                if (!in_fov(ctx, e) || !e.has_component(kDrop)) return;
                PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
                if (is_damage_petal(pid)) want_ground = true;
            });
        }
        // e) XP trashing candidate present (1.5)
        if (!want_ground) {
            ctx.sim->spatial_hash.query(ctx.cx, ctx.cy, ctx.half_w + 50, ctx.half_h + 50, [&](Simulation *sm, Entity &e){
                if (!sm->ent_alive(e.id)) return;
                if (!in_fov(ctx, e) || !e.has_component(kDrop)) return;
                PetalID::T pid = e.get_drop_id(); if (pid == PetalID::kNone) return;
                if (is_pure_heal(pid) || pid == PetalID::kLeaf) return;
                uint8_t r = rarity_of(pid);
                if (r > worst_main) return;
                if (currently_would_swap_heals && is_damage_petal(pid) && damage_of(pid) > rose_damage_threshold()) return;
                want_ground = true;
            });
        }

        // (1) Background unclog respecting reservation
        {
            int tr_j = pick_lowest_rarity_trash_slot_reserve(ctx.player, secStart, worst_main, reserved);
            if (tr_j >= 0) {
                ctx.player.set_loadout_ids(secStart + (uint8_t)tr_j, PetalID::kNone);
                // do only one per call
            }
        }

        // (2) If we want a ground drop and secondary is full, free one slot (never trash reserved/pure-heals/Leaf or upgrades)
        if (want_ground && secondary_full) {
            int tr_j = pick_lowest_rarity_trash_slot_reserve(ctx.player, secStart, worst_main, reserved);
            if (tr_j >= 0) {
                ctx.player.set_loadout_ids(secStart + (uint8_t)tr_j, PetalID::kNone);
            }
        }
    }

    // Priority 3 (inventory): Replace any lower-rarity main petal with highest-rarity from secondary (exclude heals)
    int worst_i = -1; uint8_t worst_r = 255;
    for (uint8_t i=0;i<mainN;++i) {
        PetalID::T mid = ctx.player.get_loadout_ids(i);
        if (is_heal_only(mid)) continue; // skip heals for upgrade target
        uint8_t r = r_of(mid);
        if (r < worst_r) { worst_r = r; worst_i = (int)i; }
    }
    if (worst_i < 0) {
        worst_r = 255;
        for (uint8_t i=0;i<mainN;++i) {
            uint8_t r = r_of(ctx.player.get_loadout_ids(i));
            if (r < worst_r) { worst_r = r; worst_i = (int)i; }
        }
    }
    int best_j = -1; uint8_t best_r = 0;
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
        if (sid == PetalID::kNone) continue;
        if (is_heal_only(sid) || is_pure_heal(sid)) continue; // never pull heals into main for P3 upgrade
        uint8_t r = r_of(sid);
        if (r > best_r) { best_r = r; best_j = (int)j; }
    }
    if (best_j >= 0 && worst_i >= 0 && best_r > worst_r) {
        uint8_t idx = secStart + (uint8_t)best_j;
        ctx.player.set_loadout_ids((uint8_t)worst_i, ctx.player.get_loadout_ids(idx));
        ctx.player.set_loadout_ids(idx, PetalID::kNone);
        return;
    }

    // Priority 2 (inventory): Promote damage petals into main by replacing Basics, then trash Basics in secondary
    for (uint8_t i=0;i<mainN;++i) {
        if (ctx.player.get_loadout_ids(i) != PetalID::kBasic) continue;
        for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
            uint8_t idx = secStart + j; PetalID::T sid = ctx.player.get_loadout_ids(idx);
            if (sid == PetalID::kNone || sid == PetalID::kBasic) continue;
            if (!is_damage_petal(sid) && sid != PetalID::kLeaf) continue; // allow Leaf as "better than Basic"
            ctx.player.set_loadout_ids(i, sid);
            ctx.player.set_loadout_ids(idx, PetalID::kNone); // trash basic
            return; // do one change per call
        }
    }

    // Trash Basic in secondary (one per call)
    for (uint8_t j=0;j<MAX_SLOT_COUNT;++j) {
        uint8_t idx = secStart + j;
        if (ctx.player.get_loadout_ids(idx) == PetalID::kBasic) { ctx.player.set_loadout_ids(idx, PetalID::kNone); return; }
    }
}

// Utilities (minimal for current needs)
float rarity_sum_main(Entity const &) { return 0.0f; }

bool is_heal_only(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    return (pd.attributes.burst_heal > 0 || pd.attributes.constant_heal > 0);
}

bool is_damage_petal(PetalID::T id) {
    if (id == PetalID::kNone) return false; auto const &pd = PETAL_DATA[id];
    // Treat any positive damage or body-damage as damage-capable
    return pd.damage > 0.01f || pd.attributes.extra_body_damage > 0.0f;
}

} } // namespace Bots::Priority
