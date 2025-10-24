#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Lotus(Renderer &ctx, float r) {
    ctx.scale(r / 10);
    ctx.set_fill(0xffce76db);
    ctx.set_stroke(0xffa760b1);
    ctx.set_line_width(2);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(0.00, -10.00);
    ctx.bcurve_to(1.44, -7.11, 2.05, -5.89, 1.83, -4.41);
    ctx.bcurve_to(2.72, -5.62, 4.01, -6.05, 7.07, -7.07);
    ctx.bcurve_to(6.05, -4.01, 5.62, -2.72, 4.41, -1.83);
    ctx.bcurve_to(5.89, -2.05, 7.11, -1.44, 10.00, 0.00);
    ctx.bcurve_to(7.11, 1.44, 5.89, 2.05, 4.41, 1.83);
    ctx.bcurve_to(5.62, 2.72, 6.05, 4.01, 7.07, 7.07);
    ctx.bcurve_to(4.01, 6.05, 2.72, 5.62, 1.83, 4.41);
    ctx.bcurve_to(2.05, 5.89, 1.44, 7.11, 0.00, 10.00);
    ctx.bcurve_to(-1.44, 7.11, -2.05, 5.89, -1.83, 4.41);
    ctx.bcurve_to(-2.72, 5.62, -4.01, 6.05, -7.07, 7.07);
    ctx.bcurve_to(-6.05, 4.01, -5.62, 2.72, -4.41, 1.83);
    ctx.bcurve_to(-5.89, 2.05, -7.11, 1.44, -10.00, 0.00);
    ctx.bcurve_to(-7.11, -1.44, -5.89, -2.05, -4.41, -1.83);
    ctx.bcurve_to(-5.62, -2.72, -6.05, -4.01, -7.07, -7.07);
    ctx.bcurve_to(-4.01, -6.05, -2.72, -5.62, -1.83, -4.41);
    ctx.bcurve_to(-2.05, -5.89, -1.44, -7.11, 0.00, -10.00);
    ctx.fill();
    ctx.stroke();
    ctx.set_fill(0xffa760b1);
    ctx.begin_path();
    ctx.arc(0,0,1.74);
    ctx.fill();
}
}
