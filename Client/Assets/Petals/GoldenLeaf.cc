#include <Client/Assets/Petals/Petals.hh>

void Petals::GoldenLeaf(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xffebeb34);
    ctx.set_stroke(0xffbebe2a);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(-20, 0);
    ctx.line_to(-15, 0);
    ctx.bcurve_to(-10,-12,5,-12,15,0);
    ctx.bcurve_to(5,12,-10,12,-15,0);
    ctx.fill();
    ctx.stroke();
    ctx.begin_path();
    ctx.move_to(-9,0);
    ctx.qcurve_to(0,-1.5,7.5,0);
    ctx.stroke();
}
