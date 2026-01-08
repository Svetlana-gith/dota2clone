#include "FlexLayout.h"
#include "../core/CPanel2D.h"
#include "CStyleSheet.h"
#include <algorithm>
#include <vector>
#include <spdlog/spdlog.h>
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)

namespace Panorama {

f32 ComputeJustifyOffset(JustifyContent justify, f32 remaining) {
    switch (justify) {
        case JustifyContent::Center:
            return remaining * 0.5f;
        case JustifyContent::End:
            return remaining;
        case JustifyContent::Start:
        case JustifyContent::SpaceBetween:
        default:
            return 0.0f;
    }
}

f32 AlignCross(CPanel2D* child, f32 crossSize, AlignItems align, bool isRow) {
    f32 childCrossSize = isRow ? child->GetActualHeight() : child->GetActualWidth();
    
    switch (align) {
        case AlignItems::Center:
            return (crossSize - childCrossSize) * 0.5f;
        case AlignItems::End:
            return crossSize - childCrossSize;
        case AlignItems::Stretch:
        case AlignItems::Start:
        default:
            // TODO: Account for margins in cross-axis alignment
            return 0.0f;
    }
}

LineSpacing ComputeLineSpacing(AlignContent alignContent, f32 totalCrossSize, f32 usedCrossSize, i32 lineCount) {
    LineSpacing result;
    f32 remaining = totalCrossSize - usedCrossSize;
    
    switch (alignContent) {
        case AlignContent::Center:
            result.startOffset = remaining * 0.5f;
            result.lineGap = 0.0f;
            break;
        case AlignContent::End:
            result.startOffset = remaining;
            result.lineGap = 0.0f;
            break;
        case AlignContent::SpaceBetween:
            result.startOffset = 0.0f;
            result.lineGap = (lineCount > 1) ? remaining / (lineCount - 1) : 0.0f;
            break;
        case AlignContent::SpaceAround:
            result.lineGap = remaining / lineCount;
            result.startOffset = result.lineGap * 0.5f;
            break;
        case AlignContent::Stretch:
            result.startOffset = 0.0f;
            result.lineGap = remaining / lineCount;
            break;
        case AlignContent::Start:
        default:
            result.startOffset = 0.0f;
            result.lineGap = 0.0f;
            break;
    }
    
    return result;
}

void LayoutFlex(CPanel2D* parent) {
    auto& style = parent->GetComputedStyle();
    auto& children = parent->GetChildren();
    
    // Get flex properties
    FlexDirection direction = style.flexDirection.value_or(FlexDirection::Row);
    JustifyContent justify = style.justifyContent.value_or(JustifyContent::Start);
    AlignItems align = style.alignItems.value_or(AlignItems::Start);
    FlexWrap wrap = style.flexWrap.value_or(FlexWrap::NoWrap);
    AlignContent alignContent = style.alignContent.value_or(AlignContent::Start);
    f32 gap = style.gap.value_or(0.0f);
    
    bool isRow = (direction == FlexDirection::Row);
    bool shouldWrap = (wrap != FlexWrap::NoWrap);
    
    // Get content bounds (already accounts for padding)
    const Rect2D& contentBounds = parent->GetContentBounds();
    f32 mainSize = std::max(0.0f, isRow ? contentBounds.width : contentBounds.height);
    f32 crossSize = std::max(0.0f, isRow ? contentBounds.height : contentBounds.width);
    
    // Count visible children
    i32 visibleCount = 0;
    for (auto& child : children) {
        if (child && child->IsVisible()) visibleCount++;
    }
    
    if (visibleCount == 0) return;
    
    // If mainSize is 0 (no explicit height/width), we need intrinsic sizing
    bool needsIntrinsicMainSize = (mainSize <= 0.0f);
    
    // Debug: log flex properties for form container (disabled after debugging)
    // static int flexLogCount = 0;
    // if (flexLogCount < 5 && parent->GetID().find("Form") != std::string::npos) {
    //     LOG_INFO("LayoutFlex id='{}' gap={} direction={} children={} mainSize={} crossSize={}",
    //         parent->GetID(), gap, isRow ? "row" : "column", (int)children.size(),
    //         mainSize, crossSize);
    //     flexLogCount++;
    // }
    
    // ============ Phase 1: Measure & Build Items ============
    
    struct FlexItem {
        CPanel2D* panel = nullptr;
        f32 baseSize = 0.0f;
        f32 finalSize = 0.0f;
        f32 crossAxisSize = 0.0f;
        f32 grow = 0.0f;
        f32 shrink = 1.0f;
        i32 lineIndex = 0;
    };
    
    // Helper to resolve Length values
    auto resolveLen = [&](const Length& len, f32 parentSize) -> f32 {
        switch (len.unit) {
            case LengthUnit::Pixels: return len.value;
            case LengthUnit::Percent: return parentSize * len.value / 100.0f;
            case LengthUnit::Fill: return parentSize;
            default: return len.value;
        }
    };
    
    std::vector<FlexItem> items;
    items.reserve(visibleCount);
    
    for (auto& child : children) {
        if (!child || !child->IsVisible()) continue;
        
        FlexItem item;
        item.panel = child.get();
        
        // Get flex properties from child's computed style
        const auto& childStyle = child->GetComputedStyle();
        item.grow = childStyle.flexGrow.value_or(0.0f);
        item.shrink = childStyle.flexShrink.value_or(1.0f);
        
        // Determine base size - use explicit CSS size if set, otherwise use a minimal measurement
        // For main axis (height in column layout):
        if (isRow) {
            // Row layout: main axis is width
            if (childStyle.width.has_value()) {
                item.baseSize = resolveLen(childStyle.width.value(), contentBounds.width);
            } else if (childStyle.flexBasis.has_value() && childStyle.flexBasis.value() > 0) {
                item.baseSize = childStyle.flexBasis.value();
            } else {
                // No explicit width - use content bounds width as fallback
                item.baseSize = contentBounds.width;
            }
            
            // Cross axis size (height)
            if (childStyle.height.has_value()) {
                item.crossAxisSize = resolveLen(childStyle.height.value(), contentBounds.height);
            } else {
                // Default height for elements without explicit height
                item.crossAxisSize = 48.0f; // Reasonable default
            }
        } else {
            // Column layout: main axis is height
            if (childStyle.height.has_value()) {
                item.baseSize = resolveLen(childStyle.height.value(), contentBounds.height);
            } else if (childStyle.flexBasis.has_value() && childStyle.flexBasis.value() > 0) {
                item.baseSize = childStyle.flexBasis.value();
            } else {
                // No explicit height - use a reasonable default based on element type
                // This prevents elements from stretching to fill the container
                item.baseSize = 48.0f; // Default height for form elements
            }
            
            // Cross axis size (width)
            if (childStyle.width.has_value()) {
                item.crossAxisSize = resolveLen(childStyle.width.value(), contentBounds.width);
            } else {
                // Default to full width in column layout
                item.crossAxisSize = contentBounds.width;
            }
        }
        
        item.finalSize = item.baseSize;
        items.push_back(item);
    }
    
    // If we need intrinsic main size, calculate it from children
    if (needsIntrinsicMainSize && !items.empty()) {
        f32 totalChildrenSize = 0.0f;
        for (const auto& item : items) {
            totalChildrenSize += item.baseSize;
        }
        // Add gaps between items
        totalChildrenSize += gap * (items.size() - 1);
        mainSize = totalChildrenSize;
    }
    
    // ============ Phase 2: Line Breaking (if wrap enabled) ============
    
    struct FlexLine {
        std::vector<FlexItem*> items;
        f32 mainSize = 0.0f;
        f32 crossSize = 0.0f;
    };
    
    std::vector<FlexLine> lines;
    
    if (shouldWrap) {
        // Multi-line layout
        FlexLine currentLine;
        f32 currentLineSize = 0.0f;
        
        for (auto& item : items) {
            f32 itemSizeWithGap = item.baseSize + (currentLine.items.empty() ? 0.0f : gap);
            
            // Check if item fits in current line
            if (!currentLine.items.empty() && currentLineSize + itemSizeWithGap > mainSize) {
                // Start new line
                lines.push_back(currentLine);
                currentLine = FlexLine();
                currentLineSize = 0.0f;
            }
            
            item.lineIndex = static_cast<i32>(lines.size());
            currentLine.items.push_back(&item);
            currentLineSize += item.baseSize + (currentLine.items.size() > 1 ? gap : 0.0f);
            currentLine.crossSize = std::max(currentLine.crossSize, item.crossAxisSize);
        }
        
        // Add last line
        if (!currentLine.items.empty()) {
            lines.push_back(currentLine);
        }
        
        // Reverse lines if wrap-reverse
        if (wrap == FlexWrap::WrapReverse) {
            std::reverse(lines.begin(), lines.end());
            // Update line indices
            for (size_t i = 0; i < lines.size(); ++i) {
                for (auto* item : lines[i].items) {
                    item->lineIndex = static_cast<i32>(i);
                }
            }
        }
    } else {
        // Single-line layout
        FlexLine line;
        for (auto& item : items) {
            line.items.push_back(&item);
            line.crossSize = std::max(line.crossSize, item.crossAxisSize);
        }
        lines.push_back(line);
    }
    
    // ============ Phase 3: Grow/Shrink per Line ============
    
    for (auto& line : lines) {
        f32 totalFixed = 0.0f;
        f32 totalGrow = 0.0f;
        
        for (auto* item : line.items) {
            totalFixed += item->baseSize;
            totalGrow += item->grow;
        }
        
        // Add gaps
        f32 totalGaps = gap * (line.items.size() - 1);
        totalFixed += totalGaps;
        
        // Calculate remaining space
        f32 remaining = mainSize - totalFixed;
        line.mainSize = totalFixed;
        
        if (remaining > 0 && totalGrow > 0) {
            // Grow items
            for (auto* item : line.items) {
                if (item->grow > 0) {
                    item->finalSize += remaining * (item->grow / totalGrow);
                }
            }
            line.mainSize = mainSize;
        } else if (remaining < 0) {
            // Shrink items when content overflows
            f32 totalShrink = 0.0f;
            for (auto* item : line.items) {
                totalShrink += item->shrink * item->baseSize;
            }
            
            if (totalShrink > 0) {
                for (auto* item : line.items) {
                    f32 shrinkAmount = (-remaining) * (item->shrink * item->baseSize / totalShrink);
                    item->finalSize = std::max(0.0f, item->baseSize - shrinkAmount);
                }
                // Recalculate line size
                line.mainSize = totalGaps;
                for (auto* item : line.items) {
                    line.mainSize += item->finalSize;
                }
            }
        }
    }
    
    // ============ Phase 4: Align Content (multi-line) ============
    
    f32 totalCrossSize = 0.0f;
    for (const auto& line : lines) {
        totalCrossSize += line.crossSize;
    }
    
    LineSpacing lineSpacing = ComputeLineSpacing(alignContent, crossSize, totalCrossSize, static_cast<i32>(lines.size()));
    
    // ============ Phase 5: Position Items ============
    
    f32 crossCursor = lineSpacing.startOffset;
    
    for (auto& line : lines) {
        // Calculate main-axis cursor for this line
        f32 mainCursor = 0.0f;
        
        if (justify == JustifyContent::SpaceBetween) {
            mainCursor = 0.0f;
        } else {
            f32 lineRemaining = mainSize - line.mainSize;
            mainCursor = ComputeJustifyOffset(justify, lineRemaining);
        }
        
        // Calculate space-between gap for this line
        f32 spaceBetweenGap = gap;
        if (justify == JustifyContent::SpaceBetween && line.items.size() > 1) {
            f32 usedSpace = 0.0f;
            for (auto* item : line.items) {
                usedSpace += item->finalSize;
            }
            f32 lineRemaining = mainSize - usedSpace;
            spaceBetweenGap = lineRemaining / (line.items.size() - 1);
        }
        
        // Position items in this line
        for (size_t itemIdx = 0; itemIdx < line.items.size(); ++itemIdx) {
            auto* item = line.items[itemIdx];
            CPanel2D* child = item->panel;
            
            // Determine cross-axis size for this item
            f32 itemCrossSize = line.crossSize;
            if (alignContent == AlignContent::Stretch && lines.size() > 1) {
                itemCrossSize += lineSpacing.lineGap;
            }
            
            // Calculate cross-axis position
            f32 crossPos = AlignCross(child, itemCrossSize, align, isRow);
            
            // Create bounds for child
            Rect2D childBounds = contentBounds;
            
            if (isRow) {
                childBounds.x = contentBounds.x + mainCursor;
                childBounds.y = contentBounds.y + crossCursor + crossPos;
                childBounds.width = item->finalSize;
                
                // Handle stretch
                if (align == AlignItems::Stretch) {
                    childBounds.height = itemCrossSize;
                }
            } else {
                childBounds.x = contentBounds.x + crossCursor + crossPos;
                childBounds.y = contentBounds.y + mainCursor;
                childBounds.height = item->finalSize;
                
                // Handle stretch
                if (align == AlignItems::Stretch) {
                    childBounds.width = itemCrossSize;
                }
            }
            
            // Layout child with final bounds
            child->PerformLayout(childBounds);
            
            // Advance main cursor - add gap only between items, not after last
            mainCursor += item->finalSize;
            if (itemIdx < line.items.size() - 1) {
                mainCursor += spaceBetweenGap;
            }
        }
        
        // Advance cross cursor to next line
        crossCursor += line.crossSize + lineSpacing.lineGap;
    }
}

} // namespace Panorama
