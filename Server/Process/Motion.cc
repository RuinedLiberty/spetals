#include <Server/Process.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <cmath>

void tick_entity_motion(Simulation *sim, Entity &ent) {
    if (ent.pending_delete) return;
    if (ent.slow_ticks > 0) {
        ent.speed_ratio *= 0.5;
        --ent.slow_ticks;
    }

    ent.velocity *= (1 - ent.friction);

    if (ent.projectile_decay_active) {
        float speed = ent.velocity.magnitude();
        if (ent.projectile_init_speed == 0 && speed > 0) {
            ent.projectile_init_speed = speed;
            if (ent.projectile_target_ratio == 0) ent.projectile_target_ratio = 0.58f;
        }
        float ratio = ent.projectile_target_ratio > 0 ? ent.projectile_target_ratio : 0.8f;
        float decay_ticks = 10.0f;
        float factor = powf(ratio, 1.0f / decay_ticks);
        if (ent.projectile_init_speed > 0 && speed > ratio * ent.projectile_init_speed) {
            if (speed > 0) {
                float new_mag = speed * factor;
                float target = ratio * ent.projectile_init_speed;
                if (new_mag < target) new_mag = target;
                ent.velocity.set_magnitude(new_mag);
            }
        } else if (ent.projectile_init_speed > 0) {
            float target = ratio * ent.projectile_init_speed;
            if (speed > 0) ent.velocity.set_magnitude(target);
            ent.projectile_decay_active = 0;
        }
    }

    // Apply acceleration (after scaling by speed ratio)
    ent.velocity += (ent.acceleration * ent.speed_ratio);

    // Integrate position and collision response
    ent.set_x(ent.get_x() + ent.velocity.x + ent.collision_velocity.x);
    ent.set_y(ent.get_y() + ent.velocity.y + ent.collision_velocity.y);
    ent.collision_velocity *= 0.5;
    ent.velocity += ent.collision_velocity;

    // Keep non-projectiles within arena bounds
    if (!ent.has_component(kPetal) && !ent.has_component(kWeb)) {
        ent.set_x(fclamp(ent.get_x(), ent.get_radius(), ARENA_WIDTH - ent.get_radius()));
        ent.set_y(fclamp(ent.get_y(), ent.get_radius(), ARENA_HEIGHT - ent.get_radius()));
    }

    //ent.acceleration.set(0,0);
    ent.collision_velocity.set(0,0);
    ent.speed_ratio = 1;
}