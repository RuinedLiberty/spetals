#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void AntEgg(Renderer &ctx, float r) {
    ctx.set_stroke(0xffcfc295);
    ctx.set_fill(0xfffff0b8);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
