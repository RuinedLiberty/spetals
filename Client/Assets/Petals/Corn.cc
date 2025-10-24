#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Corn(Renderer &ctx, float r) {
    ctx.scale(r / 10);
    ctx.set_fill(0xffffe419);
    ctx.set_stroke(0xffcfb914);
    ctx.set_line_width(2);
    ctx.begin_path();
    ctx.move_to(-5,8);
    ctx.qcurve_to(-15,-8,0,-8);
    ctx.qcurve_to(15,-8,5,8);
    ctx.qcurve_to(0,2,-5,8);
    ctx.fill();
    ctx.stroke();
}
}
