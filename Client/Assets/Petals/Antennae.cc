#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Antennae(Renderer &ctx, float r) {
    (void)r;
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.set_stroke(0xff333333);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.move_to(5, 12.5);
    ctx.qcurve_to(10, -2.5, 15, -12.5);
    ctx.qcurve_to(5, -2.5, 5, 12.5);
    ctx.move_to(-5, 12.5);
    ctx.qcurve_to(-10, -2.5, -15, -12.5);
    ctx.qcurve_to(-5, -2.5, -5, 12.5);
    ctx.fill();
    ctx.stroke();
}
}
