#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Stick(Renderer &ctx, float r) {
    (void)r;
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.set_line_width(7);
    ctx.set_stroke(0xff654a19);
    ctx.begin_path();
    ctx.move_to(0,10);
    ctx.line_to(0,0);
    ctx.line_to(4,-7);
    ctx.move_to(0,0);
    ctx.line_to(-6,-10);
    ctx.stroke();
    ctx.set_line_width(3);
    ctx.set_stroke(0xff7d5b1f);
    ctx.stroke();
}
}
