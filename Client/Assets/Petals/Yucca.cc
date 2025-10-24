#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Yucca(Renderer &ctx, float r) {
    ctx.set_fill(0xff74b53f);
    ctx.set_stroke(0xff5e9333);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.move_to(14,0);
    ctx.qcurve_to(0,-12,-14,0);
    ctx.qcurve_to(0,12,14,0);
    ctx.fill();
    ctx.stroke();
    ctx.set_line_width(2);
    ctx.begin_path();
    ctx.move_to(14,0);
    ctx.qcurve_to(0,-3,-14,0);
    ctx.stroke();
}
}
