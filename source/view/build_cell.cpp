// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/build_cell.hpp"
#include "kit_text.hpp"

#include <aurore/cadran/ui/cadran_kit.hpp>

#include "action/dev_build_flow.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace
{
// The kind of change: a word and a colour (the common types named,
// others as they are written).
BuildTag type_tag(const std::string& type)
{
    struct Known { const char* type; const char* key; int color; };
    // color: 0 accent, 1 amber, 2 blue, 3 purple, 4 red, 5 grey
    static const Known KNOWN[] = {
        { "feat", "dev_build/type_feat", 0 },   { "fix", "dev_build/type_fix", 1 },
        { "docs", "dev_build/type_docs", 2 },   { "perf", "dev_build/type_perf", 3 },
        { "revert", "dev_build/type_revert", 4 }, { "refactor", "dev_build/type_refactor", 5 },
        { "test", "dev_build/type_test", 5 },   { "chore", "dev_build/type_chore", 5 },
    };
    int color = 5;
    std::string text = type;
    for (const Known& k : KNOWN)
        if (type == k.type) {
            text = kit_text::str(k.key);
            color = k.color;
        }
    static const char* const colors[] = { "cadran/teal", "cadran/amber", "cadran/blue", "cadran/violet", "cadran/coral", "cadran/text3" };
    return { text, colors[color] };
}
// A pill of the boards tinted with an ink: its fill and its border are
// that ink, faint, looked up again when the theme changes.
class TagPill : public aurore::Box
{
  public:
    explicit TagPill(const char* ink)
        : aurore::Box(aurore::Axis::ROW), key(ink)
    {
        this->tint();
    }
    void onThemeChanged() override
    {
        this->tint();
        aurore::Box::onThemeChanged();
    }

  private:
    void tint()
    {
        const NVGcolor ink = aurore::Application::getTheme()[this->key];
        this->setBackgroundColor(nvgTransRGBA(ink, 30));
        this->setBorderColor(nvgTransRGBA(ink, 90));
    }
    std::string key;
};
}   // namespace

std::vector<BuildTag> build_tags(const dev_builds::Build& b, bool newer)
{
    std::vector<BuildTag> tags;
    const dev_builds::Subject s = dev_builds::parse_subject(b.title);
    if (b.kind == dev_builds::Kind::PullRequest) tags.push_back({ "#" + std::to_string(b.pr), "cadran/teal" });
    if (b.kind != dev_builds::Kind::Release) {
        if (!s.type.empty()) tags.push_back(type_tag(s.type));
        if (!s.scope.empty()) tags.push_back({ s.scope, "cadran/text3" });
        if (s.breaking) tags.push_back({ kit_text::str("dev_build/breaking"), "cadran/coral" });
        if (s.pr && b.kind != dev_builds::Kind::PullRequest) tags.push_back({ "#" + std::to_string(s.pr), "cadran/text3" });
    }
    if (b.prerelease) tags.push_back({ kit_text::str("dev_build/kind_prerelease"), "cadran/amber" });
    switch (b.ci) {
        case dev_builds::Ci::Passed: tags.push_back({ kit_text::str("dev_build/tag_ci_passed"), "cadran/teal" }); break;
        case dev_builds::Ci::Failed: tags.push_back({ kit_text::str("dev_build/tag_ci_failed"), "cadran/coral" }); break;
        case dev_builds::Ci::Running: tags.push_back({ kit_text::str("dev_build/tag_ci_running"), "cadran/amber" }); break;
        case dev_builds::Ci::Unknown: break;
    }
    // Installed or new: an icon, no word (the tabs' count of new ones has the same sparkle).
    if (dev_build_flow::installed(b)) tags.push_back({ "", "cadran/teal", "check" });
    else if (newer) tags.push_back({ "", "cadran/teal", "sparkle" });
    return tags;
}

std::string build_title(const dev_builds::Build& b)
{
    switch (b.kind) {
        case dev_builds::Kind::Release: return kit_text::str("dev_build/release", b.version);
        case dev_builds::Kind::PullRequest:
        case dev_builds::Kind::Main:
        case dev_builds::Kind::Commit: return dev_build_flow::subject(b);
    }
    return b.commit;
}

aurore::Box* tag_view(const BuildTag& tag, float font_size)
{
    // A pill of the boards, tinted with its ink.
    const float height = font_size + 10;
    auto* box = new TagPill(tag.color);
    box->setAlignItems(aurore::AlignItems::CENTER);
    box->setHeight(height);
    box->setBorderThickness(1);
    box->setCornerRadius(height / 2);
    box->setPadding(0, 10, 0, 10);
    box->setMarginRight(8);
    box->setShrink(0.0f);
    if (tag.icon) {
        CadranIcon* glyph = new CadranIcon();
        glyph->setIcon(tag.icon);
        glyph->setThemeColor("color", tag.color);
        glyph->setWidth(font_size - 2);
        glyph->setHeight(font_size - 2);
        if (!tag.text.empty()) glyph->setMarginRight(6);
        box->addView(glyph);
    }
    if (tag.text.empty()) return box;
    auto* l = new aurore::Label();
    l->setSingleLine(true);
    l->setFontSize(font_size);
    l->setThemeColor("textColor", tag.color);
    l->setText(tag.text);
    box->addView(l);
    return box;
}

BuildCell::BuildCell()
{
    // A list cell's separator and focus (RecyclerCell, as DetailCell), two lines high.
    this->setAxis(aurore::Axis::ROW);
    this->setHeight(aurore::View::AUTO);
    this->setFocusable(true);
    this->setAlignItems(aurore::AlignItems::CENTER);
    const float indent = aurore::getStyle()["aurore/listitem/descriptionIndent"];
    this->setPadding(10, indent, 10, indent);

    auto* left = new aurore::Box(aurore::Axis::COLUMN);
    left->setGrow(1.0f);
    left->setShrink(1.0f);
    left->setMarginRight(indent);
    // The title, then its marks (installed, new) right after it.
    auto* head = new aurore::Box(aurore::Axis::ROW);
    head->setAlignItems(aurore::AlignItems::CENTER);
    this->title = new aurore::Label();
    this->title->setSingleLine(true);
    this->title->setFontSize(21);
    this->title->setShrink(1.0f);
    this->title->setMarginRight(10);
    head->addView(this->title);
    this->marks = new aurore::Box(aurore::Axis::ROW);
    this->marks->setShrink(0.0f);
    head->addView(this->marks);
    left->addView(head);
    this->tags = new aurore::Box(aurore::Axis::ROW);
    this->tags->setMarginTop(6);
    left->addView(this->tags);
    this->addView(left);

    auto* right = new aurore::Box(aurore::Axis::COLUMN);
    right->setAlignItems(aurore::AlignItems::FLEX_END);
    right->setShrink(0.0f);
    this->age = new aurore::Label();
    this->age->setSingleLine(true);
    this->age->setFontSize(18);
    this->age->setThemeColor("textColor", "cadran/text2");
    this->age->setHorizontalAlign(aurore::HorizontalAlign::RIGHT);
    right->addView(this->age);
    this->commit = new aurore::Label();
    this->commit->setSingleLine(true);
    this->commit->setFontSize(18);
    this->commit->setThemeColor("textColor", "cadran/text3");
    this->commit->setHorizontalAlign(aurore::HorizontalAlign::RIGHT);
    this->commit->setMarginTop(4);
    right->addView(this->commit);
    this->addView(right);
}

void BuildCell::set(const dev_builds::Build& b, bool newer)
{
    this->title->setText(build_title(b));
    this->marks->clearViews();
    this->tags->clearViews();
    // An icon alone (installed, new) marks the title; the words go below it,
    // and a release with none leaves no empty line.
    for (const BuildTag& t : build_tags(b, newer)) (t.text.empty() ? this->marks : this->tags)->addView(tag_view(t));
    this->tags->setVisibility(this->tags->getChildren().empty() ? aurore::Visibility::GONE : aurore::Visibility::VISIBLE);
    this->age->setText(dev_build_flow::age_text(b.date));
    // A release: its date; anything else: its commit.
    this->commit->setText(b.kind == dev_builds::Kind::Release ? b.date.substr(0, 10) : b.commit);
}
