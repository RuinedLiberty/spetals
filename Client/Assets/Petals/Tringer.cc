#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Tringer(Renderer &ctx, float r) {
    ctx.set_fill(0xff333333);
    ctx.set_stroke(0xff292929);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(r,0);
    ctx.line_to(-r*0.5f,r*0.866f);
    ctx.line_to(-r*0.5f,-r*0.866f);
    ctx.line_to(r,0);
    ctx.fill();
    ctx.stroke();
}
}
