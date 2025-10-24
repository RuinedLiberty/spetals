#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Heavy(Renderer &ctx, float r) {
    ctx.set_fill(0xffaaaaaa);
    ctx.set_stroke(0xff888888);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
