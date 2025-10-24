#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Triplet(Renderer &ctx, float r) {
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
