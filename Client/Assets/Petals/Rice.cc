#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Rice(Renderer &ctx, float r) {
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(9);
    ctx.scale(r / 13);
    ctx.begin_path();
    ctx.move_to(-8,0);
    ctx.qcurve_to(0,-3.5,8,0);
    ctx.stroke();
    ctx.set_stroke(0xffffffff);
    ctx.set_line_width(5);
    ctx.stroke();
}
}
