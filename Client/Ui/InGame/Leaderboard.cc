#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>

#include <Client/Game.hh>

#include <string>
#include <algorithm>
#include <cmath>

#include <iostream>



using namespace Ui;

static float const LEADERBOARD_WIDTH = 180;

LeaderboardSlot::LeaderboardSlot(uint8_t p) : Element(LEADERBOARD_WIDTH, 18) , pos(p) {
    ratio.set(1);
    style.animate = [&](Element *, Renderer &){
        auto &ai = Game::simulation.arena_info;
        uint32_t const player_count = ai.player_count;
        uint32_t const topN = std::min(player_count, LEADERBOARD_SIZE);

                        // Determine if we should force-show the local player in the 10th slot
        uint8_t use_self_at_10 = 0;
        if (Game::alive() && topN == LEADERBOARD_SIZE) {
            float const my_score = (float) Game::score;
            // Check presence by entity id (not name)
            uint8_t found = 0;
            for (uint32_t i = 0; i < topN; ++i) {
                if (ai.ids[i] == Game::player_id) { found = 1; break; }
            }
            if (!found && my_score <= (float) ai.scores[topN - 1]) use_self_at_10 = 1;
        }


        // If this slot is not going to render anything, skip animating
        if (pos >= topN && !(use_self_at_10 && pos == LEADERBOARD_SIZE - 1)) return;

        // Compute target ratio for this slot
        float const max_score = ((float) ai.scores[0] != 0) ? (float) ai.scores[0] : 1.0f;
        float r = 1.0f;
        if (use_self_at_10 && pos == LEADERBOARD_SIZE - 1 && Game::alive()) {
            float const my_score = (float) Game::score;
            r = my_score / max_score;
        } else {
            r = (float) ai.scores[pos] / max_score;
        }
        ratio.set(r);
        ratio.step(Ui::lerp_amount);
    };
}


void LeaderboardSlot::on_render(Renderer &ctx) {
    auto &ai = Game::simulation.arena_info;
    uint32_t const player_count = ai.player_count;
    uint32_t const topN = std::min(player_count, LEADERBOARD_SIZE);

        // Determine if we should force-show the local player in the 10th slot
        uint8_t use_self_at_10 = 0;
        float player_score = 0;
    if (Game::alive()) {
        player_score = (float) Game::score;
        if (topN == LEADERBOARD_SIZE) {
            uint8_t found = 0;
            for (uint32_t i = 0; i < topN; ++i) {
                if (ai.ids[i] == Game::player_id) { found = 1; break; }
            }
            if (!found && player_score <= (float) ai.scores[topN - 1]) use_self_at_10 = 1;
        }
    }



    // Early exit only if this slot will not display anything
    if (pos >= topN && !(use_self_at_10 && pos == LEADERBOARD_SIZE - 1)) return;

    // Background bar
    ctx.set_stroke(0xff222222);
    ctx.set_line_width(height);
    ctx.round_line_cap();
    ctx.begin_path();
    ctx.move_to(-(width-height)/2,0);
    ctx.line_to((width-height)/2,0);
    ctx.stroke();

    // Determine if this row is the local player and pick display data
        bool is_self_row = false;
    std::string name_str;
    float score_val = 0.0f;

    if (use_self_at_10 && pos == LEADERBOARD_SIZE - 1 && Game::alive()) {
        // Force show the local player in 10th slot
        is_self_row = true;
        // Use the player entity's current name to avoid ambiguity
        std::string pname = Game::simulation.get_ent(Game::player_id).get_name();
        name_str = pname.size() == 0 ? "Unnamed" : pname;
        score_val = player_score;
    } else {
        // Regular top list entry
        name_str = ai.names[pos].size() == 0 ? "Unnamed" : ai.names[pos];
        score_val = (float) ai.scores[pos];
        if (Game::alive()) {
            if (ai.ids[pos] == Game::player_id) is_self_row = true;
        }
    }


    // Foreground progress bar: player in yellow, others in the same green as header
        uint32_t const other_green = 0xff55bb55;
    uint32_t const self_yellow = FLOWER_COLORS[ColorID::kYellow];
    ctx.set_stroke(is_self_row ? self_yellow : other_green);

    ctx.set_line_width(height * 0.8f);
    ctx.begin_path();
    ctx.move_to(-(width-height)/2,0);
    ctx.line_to(-(width-height)/2+(width-height)*((float) ratio),0);
    ctx.stroke();

    // Label text
    std::string const format_string = std::format("{} - {}", name_str, format_score(score_val));
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xff222222);
    ctx.center_text_align();
    ctx.center_text_baseline();
    ctx.set_text_size(height * 0.75f);
    ctx.set_line_width(height * 0.75f * 0.12f);
    ctx.stroke_text(format_string.c_str());
    ctx.fill_text(format_string.c_str());
}


Element *Ui::make_leaderboard() {
    Container *lb_header = new Ui::Container({
        new Ui::DynamicText(18, [](){
            std::string format_string{"1 Flower"};
            if (Game::simulation.arena_info.player_count != 1) 
                format_string = std::format("{} Flowers", Game::simulation.arena_info.player_count);
            return format_string;
        })
    }, LEADERBOARD_WIDTH + 20, 48, { .fill = 0xff55bb55, .line_width = 6, .round_radius = 7 });

    Element *leaderboard = new Ui::VContainer({
        lb_header,
        new Ui::VContainer(
            Ui::make_range(0, LEADERBOARD_SIZE, [](uint32_t i){ return (Element *) (new Ui::LeaderboardSlot(i)); })
        , 10, 4, {})
    }, 0, 0, { 
        .fill = 0xff555555,
        .line_width = 6,
        .round_radius = 7,
        .should_render = [](){ return Game::should_render_game_ui(); },
        .no_polling = 1
    });
    leaderboard->style.h_justify = Style::Right;
    leaderboard->style.v_justify = Style::Top;
    leaderboard->x = -20;
    leaderboard->y = 20;
    return leaderboard;
}