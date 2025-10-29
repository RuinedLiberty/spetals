#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Game.hh>
#include <Client/Assets/Assets.hh>

#include <Shared/Entity.hh>

static void render_bot_debug_inventory(Renderer &ctx, Entity const &player, float scale) {
    // Draw a compact two-row inventory preview under bot for QA only
    float boxW = 26 * scale; float boxH = 22 * scale; float gap = 3 * scale;
    uint8_t n1 = player.get_loadout_count();
    // primary bar
    for (uint8_t i = 0; i < n1; ++i) {
        float x = (boxW+gap) * (i - n1/2.0f);
        float y = 8;
        ctx.set_fill(0x80000000);
        ctx.fill_rect(x - boxW/2, y - boxH/2, boxW, boxH);
        RenderContext r(&ctx);
        ctx.translate(x, y);
        ctx.scale(0.35f);
        draw_static_petal(player.get_loadout_ids(i), ctx);
    }
    // secondary bar
    for (uint8_t i = 0; i < MAX_SLOT_COUNT; ++i) {
        float x = (boxW+gap) * (i - MAX_SLOT_COUNT/2.0f);
        float y = 8 + boxH + gap;
        ctx.set_fill(0x40000000);
        ctx.fill_rect(x - boxW/2, y - boxH/2, boxW, boxH);
        RenderContext r(&ctx);
        ctx.translate(x, y);
        ctx.scale(0.30f);
        draw_static_petal(player.get_loadout_ids(n1 + i), ctx);
    }
}

void render_name(Renderer &ctx, Entity const &ent) {
    if (!ent.get_nametag_visible()) return;
    if (ent.id == Game::player_id) return;
    {
        RenderContext r(&ctx);
        ctx.translate(0, -ent.get_radius() - 18);
        ctx.set_global_alpha(1 - ent.deletion_animation);
        ctx.scale(1 + 0.5 * ent.deletion_animation);
        ctx.draw_text(ent.get_name().c_str(), { .size = 18 });
    }

    // Show mini inventory layout below bots only, controlled by server flag
        if (ENABLE_BOT_INVENTORY_OVERLAY
        && ent.has_component(kFlower)
        && Game::simulation.ent_exists(ent.get_parent())
        && ent.get_parent() != Game::camera_id
        && Game::simulation.get_ent(ent.get_parent()).has_component(kCamera)) {

        RenderContext r(&ctx);
        ctx.translate(0, ent.get_radius() + 24);
        render_bot_debug_inventory(ctx, ent, 1.0f);
    }
}