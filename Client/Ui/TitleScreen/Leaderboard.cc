#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Game.hh>

#include <Shared/StaticData.hh>

#include <algorithm>
#include <string>
#include <format>
#include <emscripten.h>

using namespace Ui;

static constexpr uint32_t ACCOUNT_LB_SIZE = 10;
static constexpr float ACCOUNT_LB_WIDTH = 200.0f;

extern "C" {
    EM_JS(void, update_account_leaderboard, (), {
        if (typeof fetch !== 'function') return;
        function calcNeed(level){ level = level|0; var M = (Module && Module.ACCOUNT_XP_MULT)|0; if (!M) M = 100; return (((Math.pow(1.06, level - 1) * level) + 3) * M) | 0; }
        if (!Module._acctLbTimer) {
            const log = function(){};
            Module._acctLbPolls = 0;
            const fetchIt = async () => {
                Module._acctLbPolls = (Module._acctLbPolls|0) + 1;
                const tryFetch = async (url) => {
                    try {
                        log('fetch', url);
                        const r = await fetch(url, { credentials: 'include' });
                        if (!r || !r.ok) { log('fetch fail', url, r && r.status); return null; }
                        const j = await r.json();
                        log('fetch ok', url, Array.isArray(j) ? ('len=' + j.length) : j);
                        return j;
                    } catch (e) { log('fetch error', url, e && e.message); return null; }
                };
                let data = await tryFetch('/api/leaderboard/accounts');
                if (!data) data = await tryFetch('/api/leaderboard');
                if (!data) data = await tryFetch('/auth/leaderboard');
                let list = Array.isArray(data) ? data.slice() : [];
                // ensure self is included if logged in, even if not top N
                const me = await tryFetch('/api/me');
                if (me && (me['username'] || me['discord_id'])) {
                    const selfName = String(me['username'] || me['discord_id'] || 'Unnamed');
                    const selfXp = (me['account_xp']|0) || 0;
                    let selfLevel = (me['account_level']|0) || 1;
                    // Fallback if account_level missing
                    if (!me['account_level']) {
                        // derive level from XP approximately
                        selfLevel = 1; let left = selfXp|0; while (selfLevel < 99) { const need = calcNeed(selfLevel); if (left < need) break; left -= need; selfLevel++; }
                    }
                    const selfXpNeeded = ((me['xpNeeded']|0) || calcNeed(selfLevel));
                    const idx = list.findIndex(e => (String(e['name']||e['username']||e['user']||e['global_name']||"") === selfName));
                    const row = { name: selfName, level: selfLevel|0, xp: (selfXp % Math.max(1,selfXpNeeded))|0, xpNeeded: selfXpNeeded|0, self: true };
                    if (idx === -1) { list.push(row); }
                    else { list[idx] = row; }
                } else {
                    // not logged in
                }
                try {
                    const mapped = list.map(e => ({
                        name: String(e['name'] || e['username'] || e['user'] || e['global_name'] || 'Unnamed'),
                        level: (e['level']|0),
                        xp: (e['xp']|0),
                        xpNeeded: (e['xpNeeded']|0) || (e['xp_needed']|0) || calcNeed((e['level']|0) || 1),
                        self: !!e['self']
                    }));
                    mapped.sort((a,b)=> (b.level - a.level) || (b.xp - a.xp));
                    let top = mapped.slice(0, 10);
                    const selfIdx = mapped.findIndex(r => r && r.self);
                    const inTop = selfIdx >= 0 && selfIdx < 10;
                    if (!inTop && selfIdx >= 0) top.push(mapped[selfIdx]);
                    Module.accountLeaderboard = top;
                } catch(err) { log('map/sort error', err && (err.stack||err.message)); }
            };
            Module._acctLbFetch = fetchIt;
            Module._acctLbTimer = setInterval(fetchIt, 3000);
            try { if (typeof ENV !== 'undefined' && ENV && ENV.ACCOUNT_XP_MULT) Module.ACCOUNT_XP_MULT = (ENV.ACCOUNT_XP_MULT|0); } catch(e) {}
            try { if (!Module.ACCOUNT_XP_MULT && typeof process !== 'undefined' && process && process.env && process.env.ACCOUNT_XP_MULT) Module.ACCOUNT_XP_MULT = (parseInt(process.env.ACCOUNT_XP_MULT,10)||100); } catch(e) {}
            if (!Module.ACCOUNT_XP_MULT) Module.ACCOUNT_XP_MULT = 100;
            fetchIt();
        }
    });

    EM_JS(void, fetch_account_leaderboard_now, (), {
        try { if (Module._acctLbFetch) Module._acctLbFetch(); } catch(e) {}
    });

    EM_JS(int, get_account_lb_count, (), {
        return (Module.accountLeaderboard && Module.accountLeaderboard.length) | 0;
    });

    EM_JS(char*, get_account_lb_name, (int i), {
        const s = (Module.accountLeaderboard && Module.accountLeaderboard[i] && Module.accountLeaderboard[i].name) || "";
        const len = lengthBytesUTF8(s) + 1;
        const p = _malloc(len);
        stringToUTF8(s, p, len);
        return p;
    });

    EM_JS(int, get_account_lb_level, (int i), {
        return (Module.accountLeaderboard && Module.accountLeaderboard[i] && (Module.accountLeaderboard[i].level|0)) | 0;
    });

    EM_JS(int, get_account_lb_xp, (int i), {
        return (Module.accountLeaderboard && Module.accountLeaderboard[i] && (Module.accountLeaderboard[i].xp|0)) | 0;
    });

    EM_JS(int, get_account_lb_xp_needed, (int i), {
        return (Module.accountLeaderboard && Module.accountLeaderboard[i] && (Module.accountLeaderboard[i].xpNeeded|0)) | 0;
    });

    EM_JS(int, get_account_lb_is_self, (int i), {
        return (Module.accountLeaderboard && Module.accountLeaderboard[i] && (Module.accountLeaderboard[i].self ? 1 : 0)) | 0;
    });
}

namespace Ui {
    class AccountLeaderboardSlot final : public Element {
    public:
        uint8_t pos;
        LerpFloat ratio;
        AccountLeaderboardSlot(uint8_t p) : Element(ACCOUNT_LB_WIDTH, 18), pos(p) {
            ratio.set(0);
            style.no_polling = 1;
            style.animate = [&](Element*, Renderer&){
                int count = get_account_lb_count();
                if (pos >= (uint8_t)std::min(count, (int)ACCOUNT_LB_SIZE)) return;
                // Compute relative total XP value for this entry vs the max among visible entries
                auto total_for = [](int idx)->double{
                    int lvl = get_account_lb_level(idx);
                    int xp = get_account_lb_xp(idx);
                    if (lvl < 1) lvl = 1;
                    double total = (double)xp;
                    for (int l = 1; l < lvl; ++l) total += (double)score_to_pass_level(l) * (double)ACCOUNT_XP_MULTIPLIER;
                    return total;
                };
                int showN = std::min(count, (int)ACCOUNT_LB_SIZE);
                if (pos >= (uint8_t)showN) return;
                double maxv = 1.0;
                for (int i = 0; i < showN; ++i) {
                    double v = total_for(i);
                    if (i == 0 || v > maxv) maxv = v;
                }
                double cur = total_for(pos);
                float r = (maxv <= 0.0) ? 0.0f : (float)(cur / maxv);
                r = std::min(1.0f, std::max(0.0f, r));
                ratio.set(r);
                ratio.step(Ui::lerp_amount);
            };
        }
        virtual void on_render(Renderer &ctx) override {
            int count = get_account_lb_count();
            if (pos >= (uint8_t)std::min(count, (int)ACCOUNT_LB_SIZE)) return;

            // background bar
            ctx.set_stroke(0xff222222);
            ctx.set_line_width(height);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(-(width-height)/2,0);
            ctx.line_to((width-height)/2,0);
            ctx.stroke();

            // progress bar (xp towards next level): local account in yellow, others green
            bool is_self = get_account_lb_is_self(pos) != 0;
            uint32_t stroke_color = is_self ? 0xffe6d34a : 0xff55bb55;
            ctx.set_stroke(stroke_color);
            ctx.set_line_width(height * 0.8f);
            ctx.begin_path();
            ctx.move_to(-(width-height)/2,0);
            ctx.line_to(-(width-height)/2 + (width-height) * (float)ratio, 0);
            ctx.stroke();

            // label text: "#N name (Lv X)"
            char *name_c = get_account_lb_name(pos);
            std::string name = name_c ? std::string(name_c) : std::string();
            if (name_c) free(name_c);
            int level = get_account_lb_level(pos);
            std::string text = std::format("#{} {} (Lv {})", (int)pos + 1, name.size() ? name : std::string("Unnamed"), level);

            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xff222222);
            ctx.center_text_align();
            ctx.center_text_baseline();
            ctx.set_text_size(height * 0.75f);
            ctx.set_line_width(height * 0.75f * 0.12f);
            ctx.stroke_text(text.c_str());
            ctx.fill_text(text.c_str());
        }
    };
}

Element *Ui::make_title_account_leaderboard() {
    // Ensure background fetch is running
    update_account_leaderboard();

    Element *header = new Ui::DynamicText(18, [](){
        int n = get_account_lb_count();
        if (n <= 0) return std::string("Leaderboard");
        return std::format("Leaderboard ({})", n);
    });

    Container *lb = new Ui::Container({ header }, ACCOUNT_LB_WIDTH + 20, 48, { .fill = 0xff55bb55, .line_width = 6, .round_radius = 7 });

    Element *list = new Ui::VContainer(
        Ui::make_range(0, ACCOUNT_LB_SIZE, [](uint32_t i){ return (Element*) new Ui::AccountLeaderboardSlot((uint8_t)i); })
    , 10, 4, {});

    Element *board = new Ui::VContainer({ lb, list }, 0, 0, {
        .fill = 0xff555555,
        .line_width = 6,
        .round_radius = 7,
        .should_render = [](){ return Game::should_render_title_ui(); },
        .no_polling = 1
    });
    board->style.h_justify = Style::Right;
    board->style.v_justify = Style::Top;
    board->x = -20;
    board->y = 20;
    return board;
}
