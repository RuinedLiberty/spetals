#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Missile(Renderer &ctx, float r) {
    ctx.scale(r / 10);
    ctx.set_fill(0xff222222);
    ctx.set_stroke(0xff222222);
    ctx.set_line_width(5.0);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(11.0, 0.0);
    ctx.line_to(-11.0, -6.0);
    ctx.line_to(-11.0, 6.0);
    ctx.line_to(11.0, 0.0);
    ctx.fill();
    ctx.stroke();
}
}
