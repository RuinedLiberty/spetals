#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Dandelion(Renderer &ctx, float r) {
    // Stem/line
    ctx.set_stroke(0xff222222);
    ctx.round_line_cap();
    ctx.set_line_width(7);
    ctx.begin_path();
    ctx.move_to(0,0);
    ctx.line_to(-1.6f * r, 0);
    ctx.stroke();
    // Base circle (falls through to basic circle in original)
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
