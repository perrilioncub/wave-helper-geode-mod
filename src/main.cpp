#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include <unordered_set>

#include "MirrorMath.hpp"

using namespace geode::prelude;
using namespace wave_helper;

namespace {

// GD stores a slope's orientation as one of 8 directions (see
// GameObject::determineSlopeDirection). This maps each one to the corner of
// the hitbox that holds the right angle, i.e. the solid corner.
Corner cornerForSlopeDirection(int direction) {
    switch (direction) {
        case 0: case 7: return Corner::BottomRight;
        case 1: case 5: return Corner::TopRight;
        case 3: case 6: return Corner::TopLeft;
        default: return Corner::BottomLeft;
    }
}

bool isSolidType(GameObject* obj) {
    return obj->m_objectType == GameObjectType::Solid || obj->m_objectType == GameObjectType::Slope;
}

bool isHazardType(GameObject* obj) {
    return obj->m_objectType == GameObjectType::Hazard || obj->m_objectType == GameObjectType::AnimatedHazard;
}

bool isTriggerLike(GameObject* obj) {
    constexpr int START_POS_ID = 31;
    return obj->m_isTrigger || obj->m_objectID == START_POS_ID;
}

Shape shapeFor(GameObject* obj) {
    auto const& rect = obj->getObjectRect();
    Shape shape { rect.getMinX(), rect.getMinY(), rect.getMaxX(), rect.getMaxY(), Corner::None };
    if (obj->m_objectType == GameObjectType::Slope) {
        obj->determineSlopeDirection();
        shape.corner = cornerForSlopeDirection(obj->m_slopeDirection);
    }
    return shape;
}

// The objects that define the surface, best match first: real solid blocks
// and slopes, then no-touch art blocks, then hazards for spike-only floors.
std::vector<Shape> collectSurfaceShapes(std::vector<GameObject*> const& objects) {
    std::vector<Shape> tiers[3];
    for (auto obj : objects) {
        if (isSolidType(obj)) {
            bool collides = !obj->m_isNoTouch && !obj->m_isPassable;
            tiers[collides ? 0 : 1].push_back(shapeFor(obj));
        }
        else if (isHazardType(obj)) {
            tiers[2].push_back(shapeFor(obj));
        }
    }
    for (auto& tier : tiers) {
        if (!tier.empty()) return tier;
    }
    return {};
}

void notify(std::string const& text, NotificationIcon icon) {
    Notification::create(text, icon)->show();
}

bool isEditorPaused() {
    auto scene = CCDirector::get()->getRunningScene();
    return scene && scene->getChildByType<EditorPauseLayer>(0);
}

}

class $modify(MirrorEditorUI, EditorUI) {
    struct Fields {
        CCMenuItemSpriteExtra* mirrorButton = nullptr;
    };

    void createMoveMenu() {
        EditorUI::createMoveMenu();

        auto button = this->getSpriteButton("mirror-btn.png"_spr, menu_selector(MirrorEditorUI::onMirrorToCeiling), nullptr, 0.9f);
        button->setID("mirror-to-ceiling-button"_spr);
        m_fields->mirrorButton = button;
        m_editButtonBar->m_buttonArray->addObject(button);

        auto gm = GameManager::get();
        m_editButtonBar->reloadItems(
            gm->getIntGameVariable(GameVar::EditorButtonsPerRow),
            gm->getIntGameVariable(GameVar::EditorButtonRows)
        );
    }

    void updateButtons() {
        EditorUI::updateButtons();

        auto button = m_fields->mirrorButton;
        if (!button) return;
        bool hasSelection = m_selectedObject || (m_selectedObjects && m_selectedObjects->count() > 0);
        button->setEnabled(hasSelection);
        if (auto sprite = button->getChildByType<ButtonSprite>(0)) {
            ccColor3B color = hasSelection ? ccColor3B { 255, 255, 255 } : ccColor3B { 166, 166, 166 };
            for (auto child : CCArrayExt<CCNode*>(sprite->getChildren())) {
                if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(child)) {
                    rgba->setColor(color);
                }
            }
        }
    }

    void onMirrorToCeiling(CCObject*) {
        if (!m_editorLayer || m_editorLayer->m_playbackMode != PlaybackMode::Not) {
            notify("Stop the playtest first", NotificationIcon::Warning);
            return;
        }

        std::vector<GameObject*> originals;
        bool skippedTriggers = false;
        bool includeTriggers = Mod::get()->getSettingValue<bool>("include-triggers");
        for (auto obj : CCArrayExt<GameObject*>(this->getSelectedObjects())) {
            if (!includeTriggers && isTriggerLike(obj)) {
                skippedTriggers = true;
                continue;
            }
            originals.push_back(obj);
        }
        if (originals.empty()) {
            notify("Select the wave floor (or ceiling) first", NotificationIcon::Warning);
            return;
        }

        auto shapes = collectSurfaceShapes(originals);
        if (shapes.empty()) {
            notify("The selection needs at least one block, slope or spike", NotificationIcon::Error);
            return;
        }

        auto directionSetting = Mod::get()->getSettingValue<std::string>("direction");
        Direction direction;
        if (directionSetting == "Floor to ceiling") direction = Direction::FloorToCeiling;
        else if (directionSetting == "Ceiling to floor") direction = Direction::CeilingToFloor;
        else direction = SurfaceMirror::detectDirection(shapes);

        SurfaceMirror mirror(std::move(shapes), direction);

        // Work out every target before touching anything, since moving
        // objects changes the hitboxes the surface was built from.
        constexpr double BLOCK = 30.0;
        double gap = Mod::get()->getSettingValue<double>("gap") * BLOCK;
        if (direction == Direction::CeilingToFloor) gap = -gap;

        std::vector<CCPoint> targets;
        targets.reserve(originals.size());
        CCPoint originalCenter = CCPointZero;
        for (auto obj : originals) {
            auto pos = obj->getPosition();
            originalCenter = originalCenter + pos;
            targets.push_back({ pos.x, static_cast<float>(mirror.mapY(pos.x, pos.y) + gap) });
        }
        originalCenter = originalCenter / static_cast<float>(originals.size());

        // Duplicate through GD so the copies keep every property (colors,
        // groups, HSV, layers...) and one Ctrl+Z removes the whole ceiling.
        auto selection = CCArray::create();
        for (auto obj : originals) selection->addObject(obj);
        this->deselectAll();
        this->selectObjects(selection, true);
        this->onDuplicate(nullptr);

        std::unordered_set<GameObject*> originalSet(originals.begin(), originals.end());
        std::vector<Ref<GameObject>> copies;
        for (auto obj : CCArrayExt<GameObject*>(this->getSelectedObjects())) {
            if (originalSet.contains(obj)) {
                notify("Couldn't duplicate the selection", NotificationIcon::Error);
                return;
            }
            copies.push_back(obj);
        }
        if (copies.size() != originals.size()) {
            notify("Couldn't duplicate the selection (object limit?)", NotificationIcon::Error);
            return;
        }

        // GD duplicates in place and keeps the order, but if a future version
        // nudges the copies, undo that nudge so the targets still line up.
        CCPoint copyCenter = CCPointZero;
        for (auto& obj : copies) copyCenter = copyCenter + obj->getPosition();
        copyCenter = copyCenter / static_cast<float>(copies.size());
        CCPoint duplicateOffset = copyCenter - originalCenter;

        bool flipOnlyVertically = Mod::get()->getSettingValue<std::string>("orientation") == "Flip vertically";
        auto single = CCArray::create();
        for (size_t i = 0; i < copies.size(); i++) {
            auto obj = copies[i].data();

            // Rotating 180 degrees in place (a horizontal flip plus a vertical
            // flip) keeps a slope's diagonal where it was but moves its solid
            // half to the other side. That's the whole hitbox fix.
            single->removeAllObjects();
            single->addObject(obj);
            bool isSlope = obj->m_objectType == GameObjectType::Slope;
            if (isSlope || !flipOnlyVertically) {
                this->flipObjectsX(single);
            }
            this->flipObjectsY(single);
            if (isSlope) obj->determineSlopeDirection();

            auto target = targets[i] + duplicateOffset;
            this->moveObject(obj, target - obj->getPosition());
        }

        auto newSelection = CCArray::create();
        for (auto& obj : copies) newSelection->addObject(obj.data());
        this->deselectAll();
        this->selectObjects(newSelection, true);
        this->updateButtons();
        this->updateObjectInfoLabel();

        auto message = fmt::format(
            "Created {} {} object{}",
            copies.size(),
            direction == Direction::FloorToCeiling ? "ceiling" : "floor",
            copies.size() == 1 ? "" : "s"
        );
        if (skippedTriggers) message += " (triggers skipped)";
        notify(message, NotificationIcon::Success);
    }
};

$on_mod(Loaded) {
    listenForKeybindSettingPresses("mirror-keybind", [](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) return;
        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_editorUI || isEditorPaused()) return;
        static_cast<MirrorEditorUI*>(editor->m_editorUI)->onMirrorToCeiling(nullptr);
    });
}
