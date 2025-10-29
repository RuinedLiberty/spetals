#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Game.hh>
#include <Client/Ui/Extern.hh>
#include <Shared/StaticData.hh>

#include <string>

using namespace Ui;

AccountLevelBar::AccountLevelBar() : Element(260, 28) {
    progress = 0;
    level = 1;
    // Animate progress smoothly to the current account XP ratio
    style.animate = [&](Element *elt, Renderer &ctx) {
        float ratio = 0.0f;
        if (Game::account_xp_needed > 0) {
            ratio = fclamp((float)Game::account_xp / (float)Game::account_xp_needed, 0.0f, 1.0f);
        }
        progress.set(ratio);
        progress.step(Ui::lerp_amount);
        level = Game::account_level;
        // No positional translate here; parent container handles slide-in
    };
    style.h_justify = Style::Left;
    style.v_justify = Style::Bottom;
}

void AccountLevelBar::on_render(Renderer &ctx) {
    // Background bar (same style as in-game level bar)
    ctx.set_stroke(0xc0222222);
    ctx.round_line_cap();
    ctx.set_line_width(height);
    ctx.begin_path();
    ctx.move_to(-width / 2, 0);
    ctx.line_to(width / 2, 0);
    ctx.stroke();
    // Foreground progress
    ctx.set_stroke(0xfff9e496);
    ctx.set_line_width(height * 0.8f);
    ctx.begin_path();
    ctx.move_to(-width / 2, 0);
    ctx.line_to(-width / 2 + width * ((float)progress), 0);
    ctx.stroke();

    // Text inside the bar (same style as in-game)
    std::string text = std::string("Lvl ") + std::to_string(level) + " Account";
    ctx.draw_text(text.c_str(), { .size = 14 });
}
