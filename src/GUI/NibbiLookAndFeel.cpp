#include "NibbiLookAndFeel.h"
#include <cmath>

namespace nibbi::gui
{
namespace
{
void gradient (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom)
{
    g.setGradientFill ({ top, r.getCentreX(), r.getY(), bottom, r.getCentreX(), r.getBottom(), false });
}

void line (juce::Graphics& g, float x1, float y1, float x2, float y2, float width = 2.0f)
{
    g.drawLine (x1, y1, x2, y2, width);
}
}

void NibbiLookAndFeel::led (juce::Graphics& g, juce::Point<float> c, float radius,
                            juce::Colour colour, bool lit)
{
    const float power=lit?colour.getBrightness():0.f;
    const auto hue=power>0?colour.withBrightness(1.f):juce::Colour(0xff68716e);
    auto r=juce::Rectangle<float>(radius*2,radius*2).withCentre(c);
    // Soft spill remains outside the metal bezel; component bounds include it.
    if(power>0) {
        juce::ColourGradient halo(hue.withAlpha(.30f*std::sqrt(power)),c.x,c.y,
                                  hue.withAlpha(0.f),c.x+radius+7.f,c.y,true);
        halo.addColour(.52,hue.withAlpha(.15f*power));
        g.setGradientFill(halo);g.fillEllipse(r.expanded(7));
    }
    g.setColour(juce::Colours::black.withAlpha(.5f));g.fillEllipse(r.translated(0,2));
    gradient(g,r,ink::gold.brighter(.35f),ink::gold.darker(.45f));g.fillEllipse(r);
    g.setColour(ink::paper.withAlpha(.6f));g.drawEllipse(r.reduced(1),.8f);
    const auto lens=r.reduced(4.5f);
    g.setColour(juce::Colour(0xff101715));g.fillEllipse(lens.expanded(.8f));
    const float emission=std::pow(power,.65f);
    const auto glass=juce::Colour(0xff101917).interpolatedWith(hue,emission*.85f);
    const auto hot=glass.interpolatedWith(juce::Colours::white,emission*.62f);
    juce::ColourGradient dome(hot,c.x-radius*.16f,c.y-radius*.19f,
                              glass.darker(.7f),lens.getRight(),lens.getBottom(),true);
    dome.addColour(.28,glass.brighter(emission*.25f));
    dome.addColour(.68,glass.darker(.18f));
    g.setGradientFill(dome);g.fillEllipse(lens);
    // A reflected softbox and a fine lower rim make the lens look convex, even off.
    const auto reflection=juce::Rectangle<float>(lens.getWidth()*.52f,lens.getHeight()*.23f)
        .withCentre({c.x-lens.getWidth()*.13f,c.y-lens.getHeight()*.27f});
    gradient(g,reflection,juce::Colours::white.withAlpha(.38f),juce::Colours::white.withAlpha(.04f));
    g.fillEllipse(reflection);
    g.setColour(juce::Colours::white.withAlpha(.30f));
    g.fillEllipse(reflection.getX()+2,reflection.getY()+1,2,1.5f);
    juce::Path rim;rim.addCentredArc(c.x,c.y,lens.getWidth()*.47f,lens.getHeight()*.47f,0,1.85f,3.7f,true);
    g.setColour(hue.withAlpha(.12f+emission*.35f));g.strokePath(rim,juce::PathStrokeType(.8f));

}

void NibbiLookAndFeel::screw (juce::Graphics& g, float x, float y)
{
    juce::Rectangle<float> r (x - 10, y - 10, 20, 20);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (r.translated (0, 1.5f));
    gradient (g, r, juce::Colour (0xff454744), juce::Colour (0xff171918));
    g.fillEllipse (r.reduced (1));
    g.setColour (juce::Colour (0xff101211));
    line (g, x - 4, y - 1, x + 4, y + 1, 2.4f);
    line (g, x + 1, y - 4, x - 1, y + 4, 2.4f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (r.reduced (2), 0.8f);
}

void NibbiLookAndFeel::mascot (juce::Graphics& g, juce::Rectangle<float> r,
                              juce::Colour colour, bool sparkles)
{
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale (r.getWidth() / 64, r.getHeight() / 64)
                        .translated (r.getX(), r.getY()));
    g.setColour (colour);
    // Nibbi's little star sprite: a soft, asymmetric silhouette, a wink and
    // a crooked smile. Keep the face simple enough for the small keycaps.
    const juce::PathStrokeType outline(2.5f,juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);
    juce::Path body;
    body.startNewSubPath(32,7);
    body.cubicTo(35,1,38,3,40,9);
    body.lineTo(44,21);body.quadraticTo(45,24,48,24);
    body.lineTo(58,25);body.cubicTo(63,25,63,28,59,31);
    body.lineTo(48,39);body.quadraticTo(45,41,46,44);
    body.lineTo(49,55);body.cubicTo(51,61,48,63,43,59);
    body.lineTo(33,52);body.quadraticTo(30,50,27,52);
    body.lineTo(16,58);body.cubicTo(11,61,9,58,11,53);
    body.lineTo(15,42);body.quadraticTo(16,39,13,37);
    body.lineTo(5,30);body.cubicTo(1,27,2,23,7,23);
    body.lineTo(20,23);body.quadraticTo(24,23,25,20);
    body.closeSubPath();
    g.strokePath(body,outline);
    g.fillEllipse(23,30,4,6);
    juce::Path wink;wink.startNewSubPath(35,33);wink.quadraticTo(39,28,43,32);
    g.strokePath(wink,outline);
    juce::Path smile;smile.startNewSubPath(26,41);smile.cubicTo(30,47,37,47,41,39);
    g.strokePath(smile,outline);
    if (sparkles)
    {
        juce::Path twinkle;
        twinkle.startNewSubPath(11,4);twinkle.quadraticTo(11,10,17,10);
        twinkle.quadraticTo(11,10,11,16);twinkle.quadraticTo(11,10,5,10);
        twinkle.quadraticTo(11,10,11,4);twinkle.closeSubPath();
        g.fillPath(twinkle);
        g.fillEllipse(54,12,3,3);
    }
}

void NibbiLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float position, float start, float end, juce::Slider& slider)
{
    juce::ignoreUnused(position);
    auto r = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                 juce::jmin (r.getWidth(), r.getHeight())).reduced (6);
    const bool large = bool (slider.getProperties()["large"]);
    const bool sharedBezel=large && !bool(slider.getProperties()["hideSilkscreen"]);
    const auto c = r.getCentre();
    const float radius = r.getWidth() * 0.5f;
    juce::ignoreUnused(start,end); // The printed arrows are traced on the panel.
    g.setColour(juce::Colours::black.withAlpha(.75f));
    g.fillEllipse(r.reduced(sharedBezel?12.f:4.f).translated(0,sharedBezel?1.f:3.f));
    auto body=r.reduced(sharedBezel?14.f:large?7.f:5.f);
    if(!sharedBezel) {
        gradient(g,body,large?ink::gold.brighter(.2f):juce::Colour(0xff5d615e),
                        large?ink::gold.darker(.2f):juce::Colour(0xff101110));
        g.fillEllipse(body);
        g.setColour(large?ink::gold:ink::paper);g.drawEllipse(body,large?3.5f:1.7f);
        body.reduce(large?7.f:5.f,large?7.f:5.f);
    }
    gradient (g, body, large ? palette::wheel.brighter (0.15f) : juce::Colour (0xff333634),
                       large ? palette::wheel.darker (0.23f) : juce::Colour (0xff080a09));
    g.fillEllipse (body);
    if (large)
    {
        // Broad molded bevel, lit from the upper left around the matte face.
        g.setGradientFill({palette::wheel.brighter(.40f),body.getX(),body.getY(),
                           palette::wheel.darker(.40f),body.getRight(),body.getBottom(),false});
        g.fillEllipse(body);
        const auto face=body.reduced(8.f);
        gradient(g,face,palette::wheel.brighter(.08f),palette::wheel.darker(.04f));
        g.fillEllipse(face);
        g.setColour(palette::wheel.darker(.18f));g.drawEllipse(face,.8f);
        // Recessed thumb grip, offset from the axis like the physical wheel.
        const float angle=float(slider.getProperties()["thumbAngle"]);
        const auto centre=c.getPointOnCircumference(body.getWidth()*.27f,angle);
        const auto recess=juce::Rectangle<float>(body.getWidth()*.25f,body.getWidth()*.25f).withCentre(centre);
        // A continuous concave gradient: shadow inside the upper wall,
        // softer reflected light at the bottom, with a narrow molded lip.
        juce::ColourGradient hollow(palette::wheel.darker(.48f),centre.x,recess.getY(),
                                    palette::wheel.brighter(.07f),centre.x,recess.getBottom(),false);
        hollow.addColour(.28,palette::wheel.darker(.29f));
        hollow.addColour(.72,palette::wheel.darker(.12f));
        g.setGradientFill(hollow);g.fillEllipse(recess);
        gradient(g,recess,palette::wheel.darker(.4f),palette::wheel.brighter(.28f));
        g.drawEllipse(recess.reduced(.4f),.8f);
    }
    else
    {
        for (int i = 0; i < 36; ++i)
        {
            const float a = float(slider.getProperties()["thumbAngle"]) + float (i) * juce::MathConstants<float>::twoPi / 36;
            const auto p1 = c.getPointOnCircumference (radius - 14, a);
            const auto p2 = c.getPointOnCircumference (radius - 10, a);
            g.setColour (juce::Colours::white.withAlpha (i % 2 == 0 ? 0.2f : 0.06f));
            g.drawLine ({ p1, p2 }, 1);
        }
        auto cap = body.reduced (10.0f);
        gradient (g, cap, palette::knobCap.brighter (0.20f), palette::knobCap.darker (0.08f));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawEllipse (cap.reduced (1), 1);
        // An inset mark makes the endless rotation visible even on the pale cap.
        const float angle=float(slider.getProperties()["thumbAngle"]);
        const auto mark1=c.getPointOnCircumference(cap.getWidth()*.24f,angle);
        const auto mark2=c.getPointOnCircumference(cap.getWidth()*.39f,angle);
        g.setColour(juce::Colour(0xff777e77));g.drawLine({mark1,mark2},2.f);
        g.setColour(juce::Colours::white.withAlpha(.65f));
        g.drawLine({mark1.translated(1,1),mark2.translated(1,1)},.7f);
    }
    // The thumb grip tracks physical turns, independent of the parameter range.

}

void NibbiLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour& colour, bool over, bool down)
{
    const auto kind = button.getProperties()["kind"].toString();
    if(kind=="led")
    {
        const auto lightColour=juce::Colour(uint32_t(int(button.getProperties()["ledRgb"])));
        led(g,button.getLocalBounds().toFloat().getCentre(),
            0.5f*float(juce::jmin(button.getWidth(),button.getHeight()))-8.f,
            lightColour,lightColour.getBrightness()>0);
        return;
    }
    const bool dark = kind == "black";
    const bool key = dark || kind == "white";
    const bool active = down || button.getToggleState();
    auto r = button.getLocalBounds().toFloat().reduced (2);
    g.setColour (juce::Colour (0xff080909));
    g.fillRoundedRectangle (r, 5);
    r.reduce (3, 2);
    const float depth = active ? 2.0f : 7.0f;
    auto base = colour;
    if (over) base = base.brighter (0.08f);
    gradient (g, r, base.darker (0.15f), base.darker (0.55f));
    g.fillRoundedRectangle (r, 5);
    auto top = r.reduced (dark ? 3.0f : 4.0f, 2).withTrimmedBottom (depth);
    if (active) top.translate (0, 3);
    gradient (g, top, base.brighter (dark ? 0.07f : 0.2f), base);
    g.fillRoundedRectangle (top, dark ? 7.0f : 8.0f);
    g.setColour (juce::Colours::white.withAlpha (dark ? 0.04f : 0.35f));
    g.drawRoundedRectangle (top.reduced (0.6f), 7, 1);
    if (key)
    {
        const auto marker=juce::Colour(uint32_t(int(button.getProperties()["ledRgb"])));
        const auto pill = juce::Rectangle<float> (21, 10).withCentre ({ r.getCentreX(), top.getY() + 11 });
        const float power=marker.getBrightness();
        const bool lit=power>0;
        const auto hue=lit?marker.withBrightness(1.f):juce::Colour(0xff202b26);
        if(lit) {
            for(int spread=5;spread>=1;--spread) {
                g.setColour(hue.withAlpha(.024f*std::sqrt(power)));
                g.fillRoundedRectangle(pill.expanded(float(spread),float(spread)*.7f),7);
            }
        }
        g.setColour(juce::Colour(0xff090e0b));g.fillRoundedRectangle(pill,5);
        const auto lens=pill.reduced(1.5f,1);
        const auto glow=juce::Colour(0xff17201b).interpolatedWith(hue,std::pow(power,.65f));
        gradient(g,lens,glow.interpolatedWith(juce::Colours::white,lit?.35f:.03f),glow.darker(.45f));
        g.fillRoundedRectangle(lens,4);
        g.setColour(juce::Colours::white.withAlpha(lit?.25f:.10f));
        g.fillRoundedRectangle(lens.getX()+3,lens.getY()+1,lens.getWidth()-6,1.4f,.7f);
        if (active)
        {
            g.setColour (marker.withAlpha (0.5f));
            g.drawRoundedRectangle (top, 7, 2);
        }
    }

}

void NibbiLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    const auto kind = button.getProperties()["kind"].toString();
    if(kind=="led")
    {
        const auto badge=button.getProperties()["badge"].toString();
        if(badge.isEmpty()) return;
        g.setColour(juce::Colour(0xff242823));
        g.setFont(juce::FontOptions(button.getWidth()<30?9.f:13.f,juce::Font::bold));
        g.drawText(badge,button.getLocalBounds(),juce::Justification::centred);
        return;
    }
    if (kind == "white" || kind == "black") return;
    const float w = float (button.getWidth()), h = float (button.getHeight());
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::translation (0, down ? 3.0f : 0.0f));
    g.setColour (palette::glyph);
    if (kind == "record")
        mascot (g, { w * 0.22f, h * 0.22f, w * 0.56f, h * 0.56f }, palette::glyph, false);
    else if (kind == "play")
    {
        juce::Path p;
        p.addTriangle (w * 0.25f, h * 0.31f, w * 0.25f, h * 0.65f, w * 0.48f, h * 0.48f);
        g.fillPath (p);
        g.fillRoundedRectangle (w * 0.58f, h * 0.31f, 5, h * 0.34f, 2);
        g.fillRoundedRectangle (w * 0.72f, h * 0.31f, 5, h * 0.34f, 2);
    }
    else if (kind == "reverse")
    {
        juce::Path p;
        p.addCentredArc (w * 0.52f, h * 0.46f, w * 0.19f, h * 0.18f, 0,
                          -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.25f, true);
        g.strokePath (p, juce::PathStrokeType (3.7f));
        juce::Path arrow;
        arrow.addTriangle (w * 0.24f, h * 0.45f, w * 0.44f, h * 0.46f, w * 0.32f, h * 0.6f);
        g.fillPath (arrow);
        line (g, w * 0.26f, h * 0.67f, w * 0.76f, h * 0.67f, 3);
    }
}
}
