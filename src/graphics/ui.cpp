#include "graphics/ui.h"
#include <algorithm>

namespace mcgng {

// UIElement implementation

void UIElement::update(float deltaTime) {
    if (!m_visible) return;

    for (auto& child : m_children) {
        child->update(deltaTime);
    }
}

void UIElement::render() {
    if (!m_visible) return;

    for (auto& child : m_children) {
        child->render();
    }
}

bool UIElement::handleEvent(UIEvent& event) {
    if (!m_visible || !m_enabled) return false;

    // Process children in reverse order (top to bottom)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if ((*it)->handleEvent(event)) {
            return true;
        }
    }

    return false;
}

void UIElement::addChild(std::shared_ptr<UIElement> child) {
    if (child) {
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }
}

void UIElement::removeChild(UIElement* child) {
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [child](const std::shared_ptr<UIElement>& elem) {
                return elem.get() == child;
            }),
        m_children.end());
}

void UIElement::clearChildren() {
    for (auto& child : m_children) {
        child->m_parent = nullptr;
    }
    m_children.clear();
}

bool UIElement::containsPoint(int x, int y) const {
    return x >= m_x && x < m_x + m_width && y >= m_y && y < m_y + m_height;
}

// UIPanel implementation

void UIPanel::render() {
    if (!m_visible) return;

    auto& renderer = Renderer::instance();

    // Draw background
    if (m_backgroundColor.a > 0 || m_backgroundTexture != INVALID_TEXTURE) {
        if (m_backgroundTexture != INVALID_TEXTURE) {
            Rect dstRect = {m_x, m_y, m_width, m_height};
            renderer.drawTexture(m_backgroundTexture, nullptr, &dstRect);
        } else {
            renderer.setDrawColor(m_backgroundColor);
            Rect rect = {m_x, m_y, m_width, m_height};
            renderer.drawRect(rect);
        }
    }

    // Draw border
    if (m_borderWidth > 0 && m_borderColor.a > 0) {
        renderer.setDrawColor(m_borderColor);
        Rect rect = {m_x, m_y, m_width, m_height};
        renderer.drawRectOutline(rect);
    }

    // Render children
    UIElement::render();
}

// UIButton implementation

void UIButton::render() {
    if (!m_visible) return;

    auto& renderer = Renderer::instance();

    // Select texture based on state
    TextureHandle texture = m_normalTexture;
    if (!m_enabled && m_disabledTexture != INVALID_TEXTURE) {
        texture = m_disabledTexture;
    } else if (m_pressed && m_pressedTexture != INVALID_TEXTURE) {
        texture = m_pressedTexture;
    } else if (m_hovered && m_hoverTexture != INVALID_TEXTURE) {
        texture = m_hoverTexture;
    }

    // Draw button background
    if (texture != INVALID_TEXTURE) {
        Rect dstRect = {m_x, m_y, m_width, m_height};
        renderer.drawTexture(texture, nullptr, &dstRect);
    } else {
        // Default rendering without textures
        Color bgColor;
        if (!m_enabled) {
            bgColor = {100, 100, 100, 255};
        } else if (m_pressed) {
            bgColor = {50, 50, 150, 255};
        } else if (m_hovered) {
            bgColor = {80, 80, 180, 255};
        } else {
            bgColor = {60, 60, 160, 255};
        }

        renderer.setDrawColor(bgColor);
        Rect rect = {m_x, m_y, m_width, m_height};
        renderer.drawRect(rect);

        renderer.setDrawColor(Color::white());
        renderer.drawRectOutline(rect);
    }

    // Draw text (placeholder - actual text rendering requires font system)
    // For now, just draw a centered marker if there's text
    if (!m_text.empty()) {
        // TODO: Render text when font system is available
    }

    UIElement::render();
}

bool UIButton::handleEvent(UIEvent& event) {
    if (!m_visible || !m_enabled) return false;

    bool wasHovered = m_hovered;
    m_hovered = containsPoint(event.mouseX, event.mouseY);

    switch (event.type) {
        case UIEventType::MouseEnter:
        case UIEventType::MouseLeave:
            // Handled by hover state check above
            break;

        case UIEventType::MouseDown:
            if (m_hovered && event.mouseButton == 0) {
                m_pressed = true;
                event.handled = true;
                return true;
            }
            break;

        case UIEventType::MouseUp:
            if (m_pressed && event.mouseButton == 0) {
                m_pressed = false;
                if (m_hovered && m_onClick) {
                    m_onClick();
                }
                event.handled = true;
                return true;
            }
            break;

        default:
            break;
    }

    return UIElement::handleEvent(event);
}

// UILabel implementation

void UILabel::render() {
    if (!m_visible || m_text.empty()) return;

    // TODO: Render text when font system is available
    // For now, this is a placeholder

    UIElement::render();
}

// UIImage implementation

void UIImage::render() {
    if (!m_visible) return;

    auto& renderer = Renderer::instance();
    renderer.setDrawColor(m_tint);

    if (m_sprite && m_sprite->isLoaded()) {
        m_sprite->setColor(m_tint);
        m_sprite->draw(m_x, m_y);
    } else if (m_texture != INVALID_TEXTURE) {
        Rect dstRect = {m_x, m_y, m_width, m_height};
        renderer.drawTexture(m_texture, nullptr, &dstRect);
    }

    UIElement::render();
}

// UIProgressBar implementation

void UIProgressBar::render() {
    if (!m_visible) return;

    auto& renderer = Renderer::instance();

    // Draw background
    renderer.setDrawColor(m_backgroundColor);
    Rect bgRect = {m_x, m_y, m_width, m_height};
    renderer.drawRect(bgRect);

    // Draw fill
    if (m_value > 0.0f) {
        renderer.setDrawColor(m_fillColor);
        int fillWidth = static_cast<int>(m_width * m_value);
        Rect fillRect = {m_x, m_y, fillWidth, m_height};
        renderer.drawRect(fillRect);
    }

    // Draw border
    renderer.setDrawColor(m_borderColor);
    renderer.drawRectOutline(bgRect);

    UIElement::render();
}

// UIManager implementation

UIManager& UIManager::instance() {
    static UIManager instance;
    return instance;
}

void UIManager::update(float deltaTime) {
    if (m_root) {
        m_root->update(deltaTime);
    }
}

void UIManager::render() {
    if (m_root) {
        m_root->render();
    }
}

bool UIManager::handleEvent(UIEvent& event) {
    if (m_root) {
        return m_root->handleEvent(event);
    }
    return false;
}

void UIManager::setFocusedElement(UIElement* element) {
    if (m_focusedElement != element) {
        if (m_focusedElement) {
            m_focusedElement->setFocused(false);
        }
        m_focusedElement = element;
        if (m_focusedElement) {
            m_focusedElement->setFocused(true);
        }
    }
}

} // namespace mcgng
