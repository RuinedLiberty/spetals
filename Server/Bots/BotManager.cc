#include <Server/Bots/BotManager.hh>

#include <Server/Spawn.hh>
#include <Server/Account/AccountLink.hh>
#include <Server/Bots/Priorities.hh>
#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Shared/StaticDefinitions.hh>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
struct BotState {
    EntityID camera;
    float target_x = 0;
    float target_y = 0;
    // reaction timing jitter
    float next_react_ms = 0;
    float last_decide_ts = 0;
    // aiming noise (mouse-like)
    float aim_dx = 0;
    float aim_dy = 0;
    float aim_sway = 0;
    // cached control output for human-like reaction hold
    float out_ax = 0;
    float out_ay = 0;
    uint8_t out_flags = 0;
};

static std::vector<BotState> g_bots;

static float frand_s() {
    return frand();
}

static BotState *find_state(EntityID camId) {
    for (auto &s : g_bots) if (s.camera == camId) return &s;
    return nullptr;
}

static void choose_new_roam_target(BotState &b, Simulation *sim) {
    // Pick a random point within arena for now; will be narrowed to within camera FOV later
    b.target_x = frand_s() * ARENA_WIDTH;
    b.target_y = frand_s() * ARENA_HEIGHT;
}

static void ensure_has_player(Simulation *sim, Entity &cam) {
    if (sim->ent_alive(cam.get_player())) return;
    // Spawn a new player and attach
    Entity &player = alloc_player(sim, cam.get_team());
    player_spawn(sim, cam, player);
    // Add a plausible name and color for disguise
    static const char* names[] = {
        "lily", "marigold", "petunia", "oakleaf", "fern", "thyme", "fleur", "hazel", "clover", "daisy"
    };
    player.set_name(names[(uint32_t)(frand_s() * (sizeof(names)/sizeof(names[0])))]);
    player.set_nametag_visible(1);
}

static void try_pickup_visible_drop(Simulation *sim, Entity &player) {
    // If any drop is within body contact we let physics/Collision handle pickup. Nothing here.
    (void)sim; (void)player;
}

} // anon

namespace Bots {

void spawn_all(Simulation *sim, uint32_t count) {
    g_bots.clear();
    if (count == 0) return;
    g_bots.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Entity &cam = alloc_cpu_camera(sim, NULL_ENTITY);
        BitMath::set(cam.flags, EntityFlags::kCPUControlled);
        // give a small random FOV variance to simulate different monitors/settings
        cam.set_fov(BASE_FOV * (0.95f + 0.1f * frand_s()));
        // spawn immediate player
        ensure_has_player(sim, cam);
        // map bot account for gallery persistence
        { std::string acc = std::string("bot:") + std::to_string((uint32_t)cam.id.id);
          AccountLink::map_camera(cam.id, acc);
          if (sim->ent_alive(cam.get_player())) AccountLink::map_player(cam.get_player(), acc);
        }
        BotState s{};
        s.camera = cam.id;
        choose_new_roam_target(s, sim);
        s.next_react_ms = 0; // react immediately on spawn
        s.last_decide_ts = 0;
        s.aim_dx = 0; s.aim_dy = 0; s.aim_sway = 0;
        g_bots.push_back(s);
    }
}

void on_tick(Simulation *sim) {
    for (BotState &b : g_bots) {
        if (!sim->ent_alive(b.camera)) continue;
        Entity &cam = sim->get_ent(b.camera);
        if (!BitMath::at(cam.flags, EntityFlags::kCPUControlled)) continue;
        ensure_has_player(sim, cam);
        { std::string acc = std::string("bot:") + std::to_string((uint32_t)cam.id.id);
          AccountLink::map_camera(cam.id, acc);
          if (sim->ent_alive(cam.get_player())) AccountLink::map_player(cam.get_player(), acc);
        }
        if (!sim->ent_alive(cam.get_player())) continue;
        Entity &player = sim->get_ent(cam.get_player());
        // Minimal priority-driven behavior: evaluate priorities and drive controls
        float half_w = 960.0f / cam.get_fov();
        float half_h = 540.0f / cam.get_fov();
        float cx = player.get_x();
        float cy = player.get_y();
        Bots::Priority::Context pctx{ sim, cam, player, half_w, half_h, cx, cy };
        auto dec = Bots::Priority::evaluate(pctx); // includes: Loot damage drop if has Basic (score 2), else Attack (score 1)
        // Inventory-only rearrangement can run concurrently with movement/attack priorities
        Bots::Priority::apply_rearrange(pctx);
        Bots::Control tmp{};
        compute_controls(sim, cam, dec, tmp);
        b.out_ax = tmp.ax; b.out_ay = tmp.ay; b.out_flags = tmp.flags;
        // Apply immediately (no reaction delay for this minimal stage)
        Vector accel(b.out_ax, b.out_ay);
        float mag = accel.magnitude();
        if (mag > PLAYER_ACCELERATION) accel.set_magnitude(PLAYER_ACCELERATION);
        player.acceleration = accel;
        player.input = b.out_flags;
    }
}

// Helper: restrict a target to the camera's on-screen rectangle (16:9)
static void clamp_to_fov_rect(Entity const &cam, float &tx, float &ty) {
    float half_w = 960.0f / cam.get_fov();
    float half_h = 540.0f / cam.get_fov();
    float cx = cam.get_camera_x();
    float cy = cam.get_camera_y();
    if (tx < cx - half_w) tx = cx - half_w;
    if (tx > cx + half_w) tx = cx + half_w;
    if (ty < cy - half_h) ty = cy - half_h;
    if (ty > cy + half_h) ty = cy + half_h;
}

// Decide if target is in effective petal hit band; emulate scissor by toggling attack
static bool should_scissor_attack(Simulation *sim, Entity const &player, Entity const &target) {
    // Estimate two ranges: defend radius ~ R0, attack radius ~ R1 (with equipment bonuses)
    // We mirror the logic from tick_player_behavior for target petal range.
    float baseR = player.get_radius() + 40.0f; // non-attacking
    float extra_range = 0.0f;
    // Third Eye or other extra range equipment is encoded in equip_flags via Flower.cc
    if (BitMath::at(player.get_equip_flags(), EquipmentFlags::kThirdEye)) extra_range = std::max(extra_range, 75.0f);
    float attackR = player.get_radius() + 100.0f + extra_range; // attacking

    Vector d(target.get_x() - player.get_x(), target.get_y() - player.get_y());
    float dist = d.magnitude();
    return (dist > baseR && dist < attackR);
}

void compute_controls(Simulation *sim, Entity &camera, Bots::Priority::Decision const &dec, Control &out_ctrl) {
    out_ctrl = {};
    if (!sim->ent_alive(camera.get_player())) return;
    Entity &player = sim->get_ent(camera.get_player());
    BotState *bs = find_state(camera.id);

    // Use player's position as the camera center immediately to avoid stale camera_x/y
    float half_w = 960.0f / camera.get_fov();
    float half_h = 540.0f / camera.get_fov();
    float cx = player.get_x();
    float cy = player.get_y();
    camera.set_camera_x(cx);
    camera.set_camera_y(cy);

    // Determine current target from decision (priority-controlled)
    EntityID best = NULL_ENTITY;
    if (dec.type == Bots::Priority::Decision::Attack && sim->ent_alive(dec.target)) {
        best = dec.target;
    } else if (sim->ent_alive(player.target)) {
        best = player.target; // keep current
    } else {
        // Fallback: first visible mob
        sim->spatial_hash.query(cx, cy, half_w + 50, half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return;
            if (e.id == player.id) return;
            if (e.has_component(kMob) && !(e.get_team() == player.get_team())) {
                if (fabsf(e.get_x() - cx) <= half_w && fabsf(e.get_y() - cy) <= half_h) {
                    if (best.null()) best = e.id; // first seen
                }
            }
        });
    }

    if (!best.null()) { player.target = best; if (bs) bs->last_decide_ts = 0; }
    else { player.target = NULL_ENTITY; if (bs) bs->last_decide_ts = 0; }

    Vector desired(0,0);
    bool attack = false; bool defend = false;

    if (dec.type == Bots::Priority::Decision::Loot && sim->ent_alive(dec.target)) {
        // Movement-first for loot even if a combat target exists
        Entity &d = sim->get_ent(dec.target);
        Vector to(d.get_x() - player.get_x(), d.get_y() - player.get_y());
        to.set_magnitude(PLAYER_ACCELERATION);
        desired = to; attack = false; defend = false;
    } else if (!best.null()) {
        Entity &t = sim->get_ent(best);
        Vector delta(t.get_x() - player.get_x(), t.get_y() - player.get_y());
        float dist = delta.magnitude();

        // Compute desired engagement distances (match petal extend/retract logic)
        float baseR = player.get_radius() + 40.0f; // passive retract radius
        float extra_range = BitMath::at(player.get_equip_flags(), EquipmentFlags::kThirdEye) ? 75.0f : 0.0f;
        float atkR = player.get_radius() + 100.0f + extra_range; // full extension
        float safeBody = player.get_radius() + t.get_radius() + 6.0f;
        float desiredDist = atkR * 0.92f; // hold just inside max extension so petals hit
        float margin = 8.0f;

        // Determine if target is stationary (improves precision by removing tangential drift)
        bool isStationary = false;
        if (t.has_component(kMob)) {
            auto const &md = MOB_DATA[t.get_mob_id()];
            isStationary = (md.attributes.stationary != 0);
        }

        // Radial controller: move fast toward/away to maintain desired distance
        Vector move(0,0);
        if (dist > desiredDist + margin * 2) {
            Vector toT = delta; toT.set_magnitude(PLAYER_ACCELERATION);
            move += toT; // close distance quickly
        } else if (dist < std::max(baseR + margin, safeBody)) {
            Vector away(player.get_x() - t.get_x(), player.get_y() - t.get_y());
            away.set_magnitude(PLAYER_ACCELERATION);
            move += away; // retreat quickly to avoid ramming
        } else {
            if (isStationary) {
                // Maintain precise distance and always add slight tangential motion (mouse-like) so we never freeze
                float err = dist - desiredDist;
                if (std::fabs(err) > margin) {
                    Vector dir = (err > 0)
                        ? Vector(t.get_x() - player.get_x(), t.get_y() - player.get_y())
                        : Vector(player.get_x() - t.get_x(), player.get_y() - t.get_y());
                    dir.set_magnitude(PLAYER_ACCELERATION * 0.6f);
                    move += dir;
                }
                // subtle constant strafe around the target
                Vector tang(-(t.get_y() - player.get_y()), (t.get_x() - player.get_x()));
                tang.set_magnitude(PLAYER_ACCELERATION * 0.15f);
                move += tang;
            } else {
                // small tangential motion for moving targets to keep sweep
                Vector tang(-(t.get_y() - player.get_y()), (t.get_x() - player.get_x()));
                tang.set_magnitude(PLAYER_ACCELERATION * 0.25f);
                move += tang;
            }
        }

        // Light avoidance from other mobs to prevent pinballing
        Vector avoid(0,0);
        sim->spatial_hash.query(cx, cy, half_w + 50, half_h + 50, [&](Simulation *sm, Entity &m){
            if (!sm->ent_alive(m.id)) return;
            if (!m.has_component(kMob)) return;
            if (m.id == t.id) return;
            if (fabsf(m.get_x() - cx) > half_w || fabsf(m.get_y() - cy) > half_h) return;
            Vector away(player.get_x() - m.get_x(), player.get_y() - m.get_y());
            float d = away.magnitude();
            float safe = player.get_radius() + m.get_radius() + 55.0f;
            if (d < safe && d > 1.0f) {
                away.set_magnitude(PLAYER_ACCELERATION * (safe - d) / safe);
                avoid += away;
            }
        });

        desired = move + avoid;
        if (desired.magnitude() > PLAYER_ACCELERATION) desired.set_magnitude(PLAYER_ACCELERATION);
        // Mouse-like micro movement: add a tiny wandering bias so we never sit perfectly still
        Vector micro;
        float microAngle = (float)player.lifetime * 0.37f / TPS + (player.id.id & 7);
        micro.unit_normal(microAngle).set_magnitude(PLAYER_ACCELERATION * 0.08f);
        desired = desired * 0.95f + micro * 0.05f;

        // Attack policy with human-like gating and scissor toggling:
        // - If too close: defend
        // - If far beyond obvious range: don't attack
        // - If in the mid-band (baseR..atkR) and target moves: scissor toggle
        // - Else: attack continuously when near enough
        if (dist < safeBody) {
            attack = false; defend = true;
        } else {
            float bandMargin = 6.0f;
            bool inBand = (dist > baseR + bandMargin) && (dist < atkR - bandMargin);
            float nearAttack = atkR + 100.0f;   // start a bit sooner like a human would
            float farNoAttack = atkR + 170.0f; // give more leeway before deciding it's too far
            bool allowAttack = (dist <= nearAttack);
            if (dist >= farNoAttack) allowAttack = false;

            if (!allowAttack) {
                attack = false; defend = false;
            } else if (inBand && !isStationary) {
                // Toggle around ~4 Hz to move petals in and out across the target distance
                uint32_t period = std::max<uint32_t>(1, TPS / 4);
                attack = ((player.lifetime / period) & 1);
                defend = false; // keep defend off while scissoring
            } else {
                attack = true; defend = false;
            }
        }
    } else {
        // No target: subtle mouse-like drift so we never appear perfectly stationary
        float roamA = (float)player.lifetime * 0.23f / TPS + (player.id.id & 3);
        desired.unit_normal(roamA).set_magnitude(PLAYER_ACCELERATION * 0.1f);
        attack = false; defend = false;
    }

    // Remove drunk jitter for precision aiming (keep zero jit wobble)

    // Human-like smoothing: limit turn rate and acceleration change per tick
    Vector prev;
    if (bs) prev.set(bs->out_ax, bs->out_ay); else prev.set(0,0);

    auto wrap_pi = [](float a){
        while (a > M_PI) a -= 2*M_PI;
        while (a < -M_PI) a += 2*M_PI;
        return a;
    };

    Vector smoothed = desired;
    float maxTurn = 0.28f; // rad per tick (~16 deg)
    float maxStep = PLAYER_ACCELERATION * 0.25f; // accel magnitude change per tick

    float prevMag = prev.magnitude();
    float desMag = desired.magnitude();

    if (prevMag > 0.01f && desMag > 0.01f) {
        float a0 = prev.angle();
        float a1 = desired.angle();
        float dA = wrap_pi(a1 - a0);
        float stepA = fclamp(dA, -maxTurn, maxTurn);
        float newA = a0 + stepA;
        // move magnitude toward desired with rate limit
        float dMag = desMag - prevMag;
        float stepMag = fclamp(dMag, -maxStep, maxStep);
        float newMag = fclamp(prevMag + stepMag, 0.0f, PLAYER_ACCELERATION);
        smoothed.unit_normal(newA).set_magnitude(newMag);
    } else {
        // If previous is tiny, blend toward desired gently
        smoothed = prev * 0.6f + desired * 0.4f;
        if (smoothed.magnitude() > PLAYER_ACCELERATION) smoothed.set_magnitude(PLAYER_ACCELERATION);
    }

    out_ctrl.ax = smoothed.x;
    out_ctrl.ay = smoothed.y;
    out_ctrl.flags = ((attack?1:0) << InputFlags::kAttacking) | ((defend?1:0) << InputFlags::kDefending);
}


void focus_all_to(Simulation *sim, float x, float y, float radius) {
    for (BotState &b : g_bots) {
        if (!sim->ent_alive(b.camera)) continue;
        Entity &cam = sim->get_ent(b.camera);
        if (!sim->ent_alive(cam.get_player())) continue;
        Entity &pl = sim->get_ent(cam.get_player());
        float dx = (frand_s()*2-1) * radius;
        float dy = (frand_s()*2-1) * radius;
        float nx = x + dx; float ny = y + dy;
        cam.set_camera_x(nx);
        cam.set_camera_y(ny);
        pl.set_x(nx); pl.set_y(ny);
    }
}

} // namespace Bots
