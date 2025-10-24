#include <Client/Assets/Petals/Petals.hh>
#include <cmath>

namespace Petals {
void Azalea(Renderer &ctx, float r) {
    ctx.set_fill(0xffff94c9);
    ctx.set_stroke(0xffcf78a3);
    ctx.set_line_width(3);
    ctx.begin_path();
    uint32_t s = 3;
    ctx.move_to(r, 0);
    for (uint32_t i = 1; i <= s; ++i) {
        float angle = i * 2 * M_PI / s;
        float angle2 = angle - M_PI / s;
        ctx.qcurve_to(2 * r * cosf(angle2), 2 * r * sinf(angle2), r * cosf(angle), r * sinf(angle));
    }
    ctx.fill();
    ctx.stroke();
}
}
