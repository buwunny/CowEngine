#include "ecs/systems/NametagSystem.hpp"

#include "ecs/Components.hpp"
#include "render/TextRenderer.hpp"

#include <algorithm>
#include <vector>

namespace ecs
{
    namespace
    {
        // Past this the label is dropped entirely; approaching it, it fades out.
        constexpr float kMaxDistance = 140.0f;
        constexpr float kFadeStart = 100.0f;
        // A label at a fixed world size is a few pixels tall across a large map,
        // so grow it with distance past this range — up to kMaxGrowth times its
        // near size. Nearby labels keep their true world size, which is what
        // makes them read as attached to the player rather than to the screen.
        constexpr float kGrowthStart = 14.0f;
        constexpr float kMaxGrowth = 3.5f;

        struct Label
        {
            float distance;
            glm::vec3 anchor;
            float height;
            glm::vec4 color;
            const std::string *text;
        };
    }

    void nametagSystem(Registry &r, TextRenderer &text, const glm::mat4 &view,
                       const glm::mat4 &projection, const glm::vec3 &camPos)
    {
        if (!text.ready())
            return;

        std::vector<Label> labels;
        auto v = r.view<Transform, Nametag>();
        for (auto e : v)
        {
            const auto &tag = v.get<Nametag>(e);
            if (tag.text.empty())
                continue;

            const auto &t = v.get<Transform>(e);
            // Net avatars sit at a zeroed model matrix until their first snapshot
            // places them; drawing then would park a label at the world origin.
            if (t.model[3][3] == 0.0f)
                continue;

            glm::vec3 anchor(t.model[3]);
            anchor.y += tag.offset;

            const float dist = glm::distance(anchor, camPos);
            if (dist > kMaxDistance)
                continue;

            const float growth = std::min(std::max(dist / kGrowthStart, 1.0f), kMaxGrowth);
            glm::vec4 color = tag.color;
            if (dist > kFadeStart)
                color.a *= 1.0f - (dist - kFadeStart) / (kMaxDistance - kFadeStart);

            labels.push_back({dist, anchor, tag.size * growth, color, &tag.text});
        }

        // Labels don't write depth, so overlapping ones blend in draw order.
        // Farthest first puts the nearest label on top, as expected.
        std::sort(labels.begin(), labels.end(),
                  [](const Label &a, const Label &b) { return a.distance > b.distance; });

        for (const Label &l : labels)
            text.drawBillboard(*l.text, l.anchor, l.height, l.color, view, projection);
    }
}
