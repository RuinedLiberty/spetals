#include <Client/Assets/Petals/Petals.hh>
#include <cmath>

namespace Petals {
void Cactus(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xff38c75f);
    ctx.set_stroke(Renderer::HSV(0xff38c75f, 0.8));
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(15,0);
    for (uint32_t i = 0; i < 8; ++i) {
        float base_angle = M_PI * 2 * i / 8;
        ctx.qcurve_to(15*0.8f*cosf(base_angle+M_PI/8),15*0.8f*sinf(base_angle+M_PI/8),15*cosf(base_angle+2*M_PI/8),15*sinf(base_angle+2*M_PI/8));
    }
    ctx.fill();
    ctx.stroke();
    ctx.set_fill(0xff74d68f);
    ctx.begin_path();
    ctx.arc(0,0,8);
    ctx.fill();
}
}
