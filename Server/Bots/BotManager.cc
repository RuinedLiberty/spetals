#include <Server/Bots/BotManager.hh>

#include <Server/Spawn.hh>
#include <Server/Account/AccountLink.hh>
#include <Server/Bots/Priorities.hh>
#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Shared/StaticDefinitions.hh>
#include <Shared/Map.hh>


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

static float frand_s() { return frand(); }

static BotState *find_state(EntityID camId) {
    for (auto &s : g_bots) if (s.camera == camId) return &s;
    return nullptr;
}

static void choose_new_roam_target(BotState &b, Simulation *sim) {
    b.target_x = frand_s() * ARENA_WIDTH;
    b.target_y = frand_s() * ARENA_HEIGHT;
}

static void ensure_has_player(Simulation *sim, Entity &cam) {
    if (sim->ent_alive(cam.get_player())) return;
    Entity &player = alloc_player(sim, cam.get_team());
    player_spawn(sim, cam, player);
    static const char* names[] = {
        "Killer x", "hi crazy", "interesting", "oakleaf", "Je suis caca", "Gator", "o", "no u", "gonziponzi", "Unnamed Flower", "hello there", "Quick Ripper", "AdE", "Miggy :D", "swarmd", "Murderer", "massiveazep1", "That one guy", "tu padre", "fatty", "that cool guy", "Akward", "aero", "Leafy", "Alma", "Amazon Box", "XxDEADxX", "jaceon", "Blur", "Blue", "nile", "i hateyouguys", "Willy Wonka", "boom", "you suck", "fuck ruined", "ANDER", "allergies", "deez nuts", "Lacros", "Amity", "FUCK U ALL", "team?", "SpareAetal?", "Hiss", "MT6621", "i am bored", "Bleach", "what me", "Narwhal sucks", "VENUS", "TRI STINGER", "Z fan", "Troll", "oof", "WARRIOR", "pls no", "Darjon", "hhhh", "flamel"
    };
    player.set_name(names[(uint32_t)(frand_s() * (sizeof(names)/sizeof(names[0])))]); // NOLINT
    player.set_nametag_visible(1);
}

static void try_pickup_visible_drop(Simulation *sim, Entity &player) {
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
        cam.set_fov(BASE_FOV * (0.95f + 0.1f * frand_s()));
        ensure_has_player(sim, cam);
        { std::string acc = std::string("bot:") + std::to_string((uint32_t)cam.id.id);
          AccountLink::map_camera(cam.id, acc);
          if (sim->ent_alive(cam.get_player())) AccountLink::map_player(cam.get_player(), acc);
        }
        BotState s{};
        s.camera = cam.id;
        choose_new_roam_target(s, sim);
        s.next_react_ms = 0;
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

        float half_w = 960.0f / cam.get_fov();
        float half_h = 540.0f / cam.get_fov();
        float cx = player.get_x();
        float cy = player.get_y();
        Bots::Priority::Context pctx{ sim, cam, player, half_w, half_h, cx, cy };
        auto dec = Bots::Priority::evaluate(pctx);
        Bots::Priority::apply_rearrange(pctx);

        Bots::Control tmp{};
        compute_controls(sim, cam, dec, tmp);
        b.out_ax = tmp.ax; b.out_ay = tmp.ay; b.out_flags = tmp.flags;

        Vector accel(b.out_ax, b.out_ay);
        float mag = accel.magnitude();
        if (mag > PLAYER_ACCELERATION) accel.set_magnitude(PLAYER_ACCELERATION);
        player.acceleration = accel;
        player.input = b.out_flags;
    }
}

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

static bool should_scissor_attack(Simulation * /*sim*/, Entity const &player, Entity const &target) {
    float baseR = player.get_radius() + 40.0f;
    float extra_range = 0.0f;
    if (BitMath::at(player.get_equip_flags(), EquipmentFlags::kThirdEye)) extra_range = std::max(extra_range, 75.0f);
    float attackR = player.get_radius() + 100.0f + extra_range;
    Vector d(target.get_x() - player.get_x(), target.get_y() - player.get_y());
    float dist = d.magnitude();
    return (dist > baseR && dist < attackR);
}

void compute_controls(Simulation *sim, Entity &camera, Bots::Priority::Decision const &dec, Control &out_ctrl) {
    out_ctrl = {};
    if (!sim->ent_alive(camera.get_player())) return;
    Entity &player = sim->get_ent(camera.get_player());
    BotState *bs = find_state(camera.id);

    float half_w = 960.0f / camera.get_fov();
    float half_h = 540.0f / camera.get_fov();
    float cx = player.get_x();
    float cy = player.get_y();
    camera.set_camera_x(cx);
    camera.set_camera_y(cy);

        EntityID best = NULL_ENTITY;

    // Determine current zone overlevel status, to suppress targets from lower zones when appropriate
    uint32_t player_level = score_to_level(player.get_score());
    uint32_t current_zone = Map::get_zone_from_pos(player.get_x(), player.get_y());
    uint32_t suitable_diff = Map::difficulty_at_level(player_level);
    auto is_overleveled_for_zone = [&](uint32_t zone_idx)->bool {
        return MAP_DATA[zone_idx].difficulty < suitable_diff;
    };

    if ((dec.type == Bots::Priority::Decision::Attack || dec.type == Bots::Priority::Decision::SeekHealMob) && sim->ent_alive(dec.target)) {
        best = dec.target;
    } else if (sim->ent_alive(player.target)) {
        best = player.target;
    } else if (dec.type != Bots::Priority::Decision::Evacuate) { // do not pick targets while evacuating
        sim->spatial_hash.query(cx, cy, half_w + 50, half_h + 50, [&](Simulation *sm, Entity &e){
            if (!sm->ent_alive(e.id)) return;
            if (e.id == player.id) return;
            if (e.has_component(kMob) && !(e.get_team() == player.get_team())) {
                if (fabsf(e.get_x() - cx) <= half_w && fabsf(e.get_y() - cy) <= half_h) {
                    uint32_t ez = Map::get_zone_from_pos(e.get_x(), e.get_y());
                    // Skip targets in any zone that is overleveled for the bot
                    if (is_overleveled_for_zone(ez)) return;
                    if (best.null()) best = e.id;
                }
            }
        });
    }

    if (!best.null() && dec.type != Bots::Priority::Decision::Evacuate) { player.target = best; if (bs) bs->last_decide_ts = 0; }
    else { player.target = NULL_ENTITY; if (bs) bs->last_decide_ts = 0; }

    Vector desired(0,0);
    bool attack = false; bool defend = false;

    if (dec.type == Bots::Priority::Decision::Loot && sim->ent_alive(dec.target)) {
        Entity &d = sim->get_ent(dec.target);
        Vector to(d.get_x() - player.get_x(), d.get_y() - player.get_y());
        to.set_magnitude(PLAYER_ACCELERATION);
        desired = to; attack = false; defend = false;
    } else if (dec.type == Bots::Priority::Decision::Evacuate) {
        // Move to the right (toward higher zone indices). Slight vertical bias to stay near center.
        desired.set(1.0f, (player.get_y() < ARENA_HEIGHT*0.45f ? 0.2f : (player.get_y() > ARENA_HEIGHT*0.55f ? -0.2f : 0.0f)));
        desired.set_magnitude(PLAYER_ACCELERATION);
        attack = false; defend = false;
    } else if (!best.null()) {

        Entity &t = sim->get_ent(best);
        Vector delta(t.get_x() - player.get_x(), t.get_y() - player.get_y());
        float dist = delta.magnitude();

        float baseR = player.get_radius() + 40.0f;
        float extra_range = BitMath::at(player.get_equip_flags(), EquipmentFlags::kThirdEye) ? 75.0f : 0.0f;
        float atkR = player.get_radius() + 100.0f + extra_range;
        float safeBody = player.get_radius() + t.get_radius() + 6.0f;
        float desiredDist = atkR * 0.92f;
        float margin = 8.0f;

        bool isStationary = false;
        if (t.has_component(kMob)) {
            auto const &md = MOB_DATA[t.get_mob_id()];
            isStationary = (md.attributes.stationary != 0);
        }

        Vector move(0,0);
        if (dist > desiredDist + margin * 2) {
            Vector toT = delta; toT.set_magnitude(PLAYER_ACCELERATION);
            move += toT;
        } else if (dist < std::max(baseR + margin, safeBody)) {
            Vector away(player.get_x() - t.get_x(), player.get_y() - t.get_y());
            away.set_magnitude(PLAYER_ACCELERATION);
            move += away;
        } else {
            if (isStationary) {
                float err = dist - desiredDist;
                if (std::fabs(err) > margin) {
                    Vector dir = (err > 0)
                        ? Vector(t.get_x() - player.get_x(), t.get_y() - player.get_y())
                        : Vector(player.get_x() - t.get_x(), player.get_y() - t.get_y());
                    dir.set_magnitude(PLAYER_ACCELERATION * 0.6f);
                    move += dir;
                }
                Vector tang(-(t.get_y() - player.get_y()), (t.get_x() - player.get_x()));
                tang.set_magnitude(PLAYER_ACCELERATION * 0.15f);
                move += tang;
            } else {
                Vector tang(-(t.get_y() - player.get_y()), (t.get_x() - player.get_x()));
                tang.set_magnitude(PLAYER_ACCELERATION * 0.25f);
                move += tang;
            }
        }

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

        Vector micro;
        float microAngle = (float)player.lifetime * 0.37f / TPS + (player.id.id & 7);
        micro.unit_normal(microAngle).set_magnitude(PLAYER_ACCELERATION * 0.08f);
        desired = desired * 0.95f + micro * 0.05f;

        if (dist < safeBody) {
            attack = false; defend = true;
        } else {
            float bandMargin = 6.0f;
            bool inBand = (dist > baseR + bandMargin) && (dist < atkR - bandMargin);
            float nearAttack = atkR + 100.0f;
            float farNoAttack = atkR + 170.0f;
            bool allowAttack = (dist <= nearAttack);
            if (dist >= farNoAttack) allowAttack = false;

            if (!allowAttack) {
                attack = false; defend = false;
            } else if (inBand && !isStationary) {
                uint32_t period = std::max<uint32_t>(1, TPS / 4);
                attack = ((player.lifetime / period) & 1);
                defend = false;
            } else {
                attack = true; defend = false;
            }
        }
    } else {
        // === PRIORITY 0.5: WANDER FULL SPEED, RIGHT-BIASED (≈60/40) ===
        float t = (float)player.lifetime / TPS;
        float theta = 0.9f * std::sin(0.8f * t + (player.id.id & 3))
                    + 0.4f * std::sin(1.7f * t + ((player.id.id>>2)&7));
        Vector randomUnit; randomUnit.unit_normal(theta);
        Vector biased = randomUnit * 0.6f + Vector(1.0f, 0.0f) * 0.4f;
        if (biased.magnitude() < 1e-3f) { biased.set(1.0f, 0.0f); }
        biased.set_magnitude(PLAYER_ACCELERATION);
        desired = biased;
        attack = false; defend = false;
    }

    Vector prev; if (bs) prev.set(bs->out_ax, bs->out_ay); else prev.set(0,0);
    auto wrap_pi = [](float a){ while (a > M_PI) a -= 2*M_PI; while (a < -M_PI) a += 2*M_PI; return a; };
    Vector smoothed = desired;

        if (dec.type != Bots::Priority::Decision::Wander && dec.type != Bots::Priority::Decision::Evacuate) {

        float maxTurn = 0.28f;
        float maxStep = PLAYER_ACCELERATION * 0.25f;
        float prevMag = prev.magnitude();
        float desMag = desired.magnitude();

        if (prevMag > 0.01f && desMag > 0.01f) {
            float a0 = prev.angle();
            float a1 = desired.angle();
            float dA = wrap_pi(a1 - a0);
            float stepA = fclamp(dA, -maxTurn, maxTurn);
            float newA = a0 + stepA;
            float dMag = desMag - prevMag;
            float stepMag = fclamp(dMag, -maxStep, maxStep);
            float newMag = fclamp(prevMag + stepMag, 0.0f, PLAYER_ACCELERATION);
            smoothed.unit_normal(newA).set_magnitude(newMag);
        } else {
            smoothed = prev * 0.6f + desired * 0.4f;
            if (smoothed.magnitude() > PLAYER_ACCELERATION) smoothed.set_magnitude(PLAYER_ACCELERATION);
        }
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