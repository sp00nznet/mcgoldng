#ifndef MCGNG_UI_H
#define MCGNG_UI_H

#include "graphics/renderer.h"
#include "graphics/sprite.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace mcgng {

/**
 * UI event types.
 */
enum class UIEventType {
    None,
    MouseEnter,
    MouseLeave,
    MouseDown,
    MouseUp,
    Click,
    DoubleClick,
    KeyDown,
    KeyUp,
    Focus,
    Blur
};

/**
 * UI event data.
 */
struct UIEvent {
    UIEventType type = UIEventType::None;
    int mouseX = 0;
    int mouseY = 0;
    int mouseButton = 0;
    int keyCode = 0;
    bool handled = false;
};

/**
 * Base UI element class.
 */
class UIElement {
public:
    UIElement() = default;
    virtual ~UIElement() = default;

    virtual void update(float deltaTime);
    virtual void render();
    virtual bool handleEvent(UIEvent& event);

    // Position and size
    void setPosition(int x, int y) { m_x = x; m_y = y; }
    void setSize(int width, int height) { m_width = width; m_height = height; }
    void setBounds(int x, int y, int width, int height) {
        m_x = x; m_y = y; m_width = width; m_height = height;
    }

    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    Rect getBounds() const { return {m_x, m_y, m_width, m_height}; }

    // Visibility
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

    // Enabled state
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Focus
    void setFocused(bool focused) { m_focused = focused; }
    bool isFocused() const { return m_focused; }

    // Parent-child relationships
    void addChild(std::shared_ptr<UIElement> child);
    void removeChild(UIElement* child);
    void clearChildren();
    const std::vector<std::shared_ptr<UIElement>>& getChildren() const { return m_children; }

    // Hit testing
    bool containsPoint(int x, int y) const;

protected:
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;

    bool m_visible = true;
    bool m_enabled = true;
    bool m_focused = false;
    bool m_hovered = false;

    UIElement* m_parent = nullptr;
    std::vector<std::shared_ptr<UIElement>> m_children;
};

/**
 * UI Panel - container element with optional background.
 */
class UIPanel : public UIElement {
public:
    void render() override;

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }
    void setBorderColor(const Color& color) { m_borderColor = color; }
    void setBorderWidth(int width) { m_borderWidth = width; }
    void setBackgroundTexture(TextureHandle texture) { m_backgroundTexture = texture; }

private:
    Color m_backgroundColor = Color::transparent();
    Color m_borderColor = Color::white();
    int m_borderWidth = 0;
    TextureHandle m_backgroundTexture = INVALID_TEXTURE;
};

/**
 * UI Button.
 */
class UIButton : public UIElement {
public:
    using ClickCallback = std::function<void()>;

    void render() override;
    bool handleEvent(UIEvent& event) override;

    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }

    void setOnClick(ClickCallback callback) { m_onClick = std::move(callback); }

    void setNormalTexture(TextureHandle texture) { m_normalTexture = texture; }
    void setHoverTexture(TextureHandle texture) { m_hoverTexture = texture; }
    void setPressedTexture(TextureHandle texture) { m_pressedTexture = texture; }
    void setDisabledTexture(TextureHandle texture) { m_disabledTexture = texture; }

private:
    std::string m_text;
    ClickCallback m_onClick;

    TextureHandle m_normalTexture = INVALID_TEXTURE;
    TextureHandle m_hoverTexture = INVALID_TEXTURE;
    TextureHandle m_pressedTexture = INVALID_TEXTURE;
    TextureHandle m_disabledTexture = INVALID_TEXTURE;

    bool m_pressed = false;
};

/**
 * UI Label - text display.
 */
class UILabel : public UIElement {
public:
    void render() override;

    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }

    void setTextColor(const Color& color) { m_textColor = color; }
    void setAlignment(int horizontal, int vertical) {
        m_hAlign = horizontal;
        m_vAlign = vertical;
    }

private:
    std::string m_text;
    Color m_textColor = Color::white();
    int m_hAlign = 0;  // 0=left, 1=center, 2=right
    int m_vAlign = 0;  // 0=top, 1=center, 2=bottom
};

/**
 * UI Image - displays a texture or sprite.
 */
class UIImage : public UIElement {
public:
    void render() override;

    void setTexture(TextureHandle texture) { m_texture = texture; }
    void setSprite(std::shared_ptr<Sprite> sprite) { m_sprite = std::move(sprite); }
    void setTint(const Color& color) { m_tint = color; }

private:
    TextureHandle m_texture = INVALID_TEXTURE;
    std::shared_ptr<Sprite> m_sprite;
    Color m_tint = Color::white();
};

/**
 * UI Progress Bar.
 */
class UIProgressBar : public UIElement {
public:
    void render() override;

    void setValue(float value) { m_value = std::max(0.0f, std::min(1.0f, value)); }
    float getValue() const { return m_value; }

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }
    void setFillColor(const Color& color) { m_fillColor = color; }
    void setBorderColor(const Color& color) { m_borderColor = color; }

private:
    float m_value = 0.0f;
    Color m_backgroundColor = Color::black();
    Color m_fillColor = Color::green();
    Color m_borderColor = Color::white();
};

/**
 * UI Manager - handles UI hierarchy and input.
 */
class UIManager {
public:
    static UIManager& instance();

    void update(float deltaTime);
    void render();

    bool handleEvent(UIEvent& event);

    void setRoot(std::shared_ptr<UIElement> root) { m_root = std::move(root); }
    UIElement* getRoot() { return m_root.get(); }

    void setFocusedElement(UIElement* element);
    UIElement* getFocusedElement() { return m_focusedElement; }

private:
    UIManager() = default;

    std::shared_ptr<UIElement> m_root;
    UIElement* m_focusedElement = nullptr;
    UIElement* m_hoveredElement = nullptr;
};

} // namespace mcgng

#endif // MCGNG_UI_H
