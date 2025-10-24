#include <Client/Assets/Petals/Petals.hh>

#include <cmath>

void Petals::Soil(Renderer &ctx, float r) {
    const float p[][2] = {
        {47.999825f,127.81531f},
        {60.856825f,122.10132f},
        {70.856825f,127.81531f},
        {72.285826f,143.52931f},
        {57.999825f,152.10131f},
        {47.999825f,149.24431f},
        {43.713825f,137.81531f}
    };
    const float minX = 43.713825f, maxX = 72.285826f;
    const float minY = 122.10132f, maxY = 152.10131f;
    const float cx = (minX + maxX) * 0.5f;
    const float cy = (minY + maxY) * 0.5f;
    const float w = (maxX - minX);
    const float h = (maxY - minY);
    const float s = (2.0f * r) / (h > w ? h : w);

    // Base fill color from SVG
    ctx.set_fill(0xff695118);
    ctx.begin_path();
    ctx.move_to((p[0][0]-cx)*s, (p[0][1]-cy)*s);
    for (int i = 1; i < 7; ++i) {
        ctx.line_to((p[i][0]-cx)*s, (p[i][1]-cy)*s);
    }
    ctx.close_path();
    ctx.fill();

    // Dark outline color
    ctx.set_stroke(0xff554213);
    ctx.set_line_width(3);
    ctx.stroke();
}
