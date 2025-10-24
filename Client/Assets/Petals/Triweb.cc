#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Triweb(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.move_to(11.00, 0.00);
    ctx.qcurve_to(4.32, 3.14, 3.40, 10.46);
    ctx.qcurve_to(-1.65, 5.08, -8.90, 6.47);
    ctx.qcurve_to(-5.34, -0.00, -8.90, -6.47);
    ctx.qcurve_to(-1.65, -5.08, 3.40, -10.46);
    ctx.qcurve_to(4.32, -3.14, 11.00, 0.00);
    ctx.fill();
    ctx.stroke();
}
}
