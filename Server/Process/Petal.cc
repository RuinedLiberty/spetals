#include <Server/Process.hh>

#include <Server/EntityFunctions.hh>
#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>

static inline void launch_projectile(Simulation *sim, Entity &proj, float angle, float lifetime_ticks, float initial_speed, float target_ratio = 0.5f) {
    proj.acceleration.set(0,0);
    proj.friction = 0.0f;
    proj.velocity.unit_normal(angle).set_magnitude(initial_speed);
    entity_set_despawn_tick(proj, (game_tick_t)lifetime_ticks);
    proj.projectile_init_speed = proj.velocity.magnitude();
    proj.projectile_target_ratio = target_ratio;
    proj.projectile_decay_active = 1;
}

void tick_petal_behavior(Simulation *sim, Entity &petal) {
    if (petal.pending_delete) return;
    if (!sim->ent_alive(petal.get_parent())) {
        sim->request_delete(petal.id);
        return;
    }
    Entity &player = sim->get_ent(petal.get_parent());
    struct PetalData const &petal_data = PETAL_DATA[petal.get_petal_id()];
    if (petal_data.attributes.rotation_style == PetalAttributes::kPassiveRot) {
        float rot_amt = petal.get_petal_id() == PetalID::kWing ? 10.0 : 1.0;
        if (petal.id.id % 2) petal.set_angle(petal.get_angle() + rot_amt / TPS);
        else petal.set_angle(petal.get_angle() - rot_amt / TPS);
        } else if (petal_data.attributes.rotation_style == PetalAttributes::kFollowRot && !(BitMath::at(petal.flags, EntityFlags::kIsDespawning))) {
        Vector delta(petal.get_x() - player.get_x(), petal.get_y() - player.get_y());
        petal.set_angle(delta.angle());
    }
    if (BitMath::at(petal.flags, EntityFlags::kIsDespawning)) {
        petal.acceleration.set(0,0);
        return;
    }
    if (petal_data.attributes.secondary_reload == 0) return;
    if (petal.secondary_reload < petal_data.attributes.secondary_reload * TPS) {
        ++petal.secondary_reload;
        return;
    }
    if (petal_data.attributes.burst_heal > 0 && player.health < player.max_health && player.dandy_ticks == 0) {
        Vector delta(player.get_x() - petal.get_x(), player.get_y() - petal.get_y());
        if (delta.magnitude() < petal.get_radius()) {
            inflict_heal(sim, player, petal_data.attributes.burst_heal);
            sim->request_delete(petal.id);
            return;
        }
        delta.set_magnitude(PLAYER_ACCELERATION * 4);
        petal.acceleration = delta;
    }
        switch (petal.get_petal_id()) {
        case PetalID::kMissile:
            if (BitMath::at(player.input, InputFlags::kAttacking)) {
                launch_projectile(sim, petal, petal.get_angle(), 1.30f * TPS, 60.0f, 0.5f);
            }
            break;
        case PetalID::kDandelion:
            if (BitMath::at(player.input, InputFlags::kAttacking)) {
                launch_projectile(sim, petal, petal.get_angle(), 1.30f * TPS, 60.0f, 0.5f);
            }
            break;
                case PetalID::kTriweb:
        case PetalID::kWeb: {
            if (BitMath::at(player.input, InputFlags::kAttacking)) {
                Vector delta(petal.get_x() - player.get_x(), petal.get_y() - player.get_y());
                float angle = delta.angle();
            if (petal.get_petal_id() == PetalID::kTriweb) angle += frand() - 0.4f;
                launch_projectile(sim, petal, angle, 0.8f * TPS, 60.0f, 0.5f);
            } else if (BitMath::at(player.input, InputFlags::kDefending))
                entity_set_despawn_tick(petal, 0.6 * TPS);
            break;
        }
        case PetalID::kBubble:
            if (BitMath::at(player.input, InputFlags::kDefending)) {
                Vector v(player.get_x() - petal.get_x(), player.get_y() - petal.get_y());
                v.set_magnitude(PLAYER_ACCELERATION * 20);
                player.velocity += v;
                sim->request_delete(petal.id);
            }
            break;
        case PetalID::kPollen:
            if (BitMath::at(player.input, InputFlags::kAttacking) || BitMath::at(player.input, InputFlags::kDefending)) {
                petal.friction = DEFAULT_FRICTION;
                entity_set_despawn_tick(petal, 4.0 * TPS);
            }
            break;
        case PetalID::kPeas:
        case PetalID::kPoisonPeas:
        case PetalID::kLPeas:
            if (BitMath::at(player.input, InputFlags::kAttacking)) {
                float spray_angle = frand() * 2 * M_PI;
                petal.set_split_projectile(0);
                petal.heading_angle = spray_angle;
                launch_projectile(sim, petal, spray_angle, 0.90f * TPS, 60.0f, 0.5f);
                for (uint32_t i = 1; i < petal_data.count; ++i) {
                    Entity &new_petal = alloc_petal(sim, petal.get_petal_id(), player);
                    new_petal.set_x(petal.get_x());
                    new_petal.set_y(petal.get_y());
                    new_petal.set_split_projectile(0);
                    new_petal.heading_angle = spray_angle + (2 * M_PI * i) / petal_data.count;
                    launch_projectile(sim, new_petal, new_petal.heading_angle, 0.90f * TPS, 60.0f, 0.5f);
                }
            }
            break;
        
        default:
            break;
    }
}