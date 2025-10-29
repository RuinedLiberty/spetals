#include <Client/Render/RenderEntity.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Render/Renderer.hh>

#include <Shared/Entity.hh>

void render_health(Renderer &ctx, Entity const &ent) {
    if (ent.has_component(kPetal)) return;
    if (ent.has_component(kMob)) return;
    bool isPlayer = ent.has_component(kFlower) && ent.id == Game::player_id;
    // Keep health bar hidden at full health, but still render account XP/level for the local player
    if (ent.healthbar_opacity < 0.01 && !isPlayer) return;
    float w = ent.get_radius() * 1.33;
    ctx.set_global_alpha((1 - ent.deletion_animation) * ent.healthbar_opacity);
    ctx.scale(1 + 0.5 * ent.deletion_animation);
    ctx.translate(-w, w + 15);
    float bar_left = 0.0f;
    float bar_right = 2 * w;


    ctx.round_line_cap();
    ctx.set_stroke(0xff222222);
    ctx.set_line_width(9);
    ctx.begin_path();
    ctx.move_to(0, 0);
    ctx.line_to(2 * w, 0);
    ctx.stroke();
    if (ent.healthbar_lag > ent.get_health_ratio()) {
        ctx.set_stroke(0xffed2f31);
        ctx.set_line_width(7);
        ctx.begin_path();
        ctx.move_to(2 * w * ent.get_health_ratio(), 0);
        ctx.line_to(2 * w * ent.healthbar_lag, 0);
        ctx.stroke();
    }
    ctx.set_stroke(0xff75dd34);
    ctx.set_line_width(7);
    ctx.begin_path();
    ctx.move_to(0, 0);
        ctx.line_to(2 * w * ent.get_health_ratio(), 0);
            ctx.stroke();

        // Account XP bar (smaller, blue), centered to the left portion of HP bar
    if (isPlayer) {
        // Ensure account bar/text are visible even if HP bar is faded out
        ctx.set_global_alpha(1.0f);
        float total_w = bar_right - bar_left;
        float xp_bar_w = total_w * 0.5f; // half the HP bar length
        float xp_bar_left = bar_left + (total_w * 0.25f) - xp_bar_w / 2.0f; // left-centered in the left half
        float xp_bar_right = xp_bar_left + xp_bar_w;
        // background
        ctx.set_stroke(0xff223a72); // dark blue-ish background
        ctx.set_line_width(6);
        ctx.begin_path();
        ctx.move_to(xp_bar_left, 10);
        ctx.line_to(xp_bar_right, 10);
        ctx.stroke();
        // progress
        float ratio = 0.0f;
        if (Game::account_xp_needed > 0)
            ratio = fclamp((float)Game::account_xp / (float)Game::account_xp_needed, 0.0f, 1.0f);
        ctx.set_stroke(0xff3aa0ff); // blue fill
        ctx.set_line_width(6);
        ctx.begin_path();
        ctx.move_to(xp_bar_left, 10);
        ctx.line_to(xp_bar_left + xp_bar_w * ratio, 10);
        ctx.stroke();

        // Account Level text, centered-right of the HP bar, never clipping into XP bar
        RenderContext r(&ctx);
        std::string txt = std::string("Lvl ") + std::to_string(Game::account_level);
        ctx.set_text_size(12);
        float tw = ctx.get_text_size(txt.c_str());
        // Position: a bit below HP bar (y = 14), at the right side but ensure at least 8px gap from xp bar
        float x_text = bar_right - tw/2.0f;
        float min_x = xp_bar_right + 8.0f + tw/2.0f;
        if (x_text < min_x) x_text = min_x;
        ctx.translate(x_text, 14);
        ctx.set_fill(0xff3aa0ff); // blue text
        ctx.draw_text(txt.c_str(), { .size = 12 });
    }
}