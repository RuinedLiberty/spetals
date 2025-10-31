#include <Client/Render/RenderEntity.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Render/Renderer.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>
#include <Client/StaticData.hh>

void render_health(Renderer &ctx, Entity const &ent) {
    if (ent.has_component(kPetal)) return;
    if (ent.has_component(kMob)) return;
    bool isLocalPlayer = ent.has_component(kFlower) && ent.id == Game::player_id;
    if (ent.healthbar_opacity < 0.01 && !isLocalPlayer) return;
    float w = ent.get_radius() * 1.33f;
    ctx.set_global_alpha((1 - ent.deletion_animation) * ent.healthbar_opacity);
    ctx.scale(1 + 0.5f * ent.deletion_animation);
    ctx.translate(-w, w + 15);

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

        if (ent.healthbar_opacity > 0.01f) {
        RenderContext rc(&ctx);
        ctx.translate(0, 14);

        uint32_t highest_rarity = 0;
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i) {
            PetalID::T pid = ent.get_loadout_ids(i);
            if (pid >= PetalID::kNumPetals) continue;
            uint8_t r = PETAL_DATA[pid].rarity;
            if (r >= RarityID::kNumRarities) continue;
            if (r > highest_rarity) highest_rarity = r;
        }
        uint32_t lvl_color = RARITY_COLORS[highest_rarity];

                uint32_t account_lvl = 1;
        if (isLocalPlayer) {
            account_lvl = Game::account_level;
        } else {
            // Use server-provided real account level when available; fallback to 1..5 for bots or unknowns
            uint32_t cached = (ent.id.id < ENTITY_CAP) ? Game::entity_account_level[ent.id.id] : 0;
            account_lvl = (cached > 0 ? cached : (1 + (ent.id.id % 5)));
        }
        std::string txt = std::string("Lvl ") + std::to_string(account_lvl);


        ctx.set_text_size(14);
        float tw = ctx.get_text_size(txt.c_str());
        float margin = 0.0f;
        ctx.translate(margin + tw * 0.5f, 0);
        ctx.draw_text(txt.c_str(), { .size = 16, .fill = lvl_color });
    }

}