#pragma once
/**
 * Flexbox Layout Engine for Panorama UI
 * CSS Flexbox-like layout system
 */

#include "../core/PanoramaTypes.h"

namespace Panorama {

class CPanel2D;

/**
 * Perform flexbox layout on a panel's children
 * @param parent Panel with display: flex enabled
 */
void LayoutFlex(CPanel2D* parent);

/**
 * Calculate cross-axis position for a child based on align-items
 * @param child Child panel to align
 * @param crossSize Available cross-axis size
 * @param align Alignment mode
 * @return Cross-axis offset
 */
f32 AlignCross(CPanel2D* child, f32 crossSize, AlignItems align, bool isRow);

/**
 * Calculate main-axis starting offset based on justify-content
 * @param justify Justification mode
 * @param remaining Remaining space after all items
 * @return Starting offset
 */
f32 ComputeJustifyOffset(JustifyContent justify, f32 remaining);

/**
 * Calculate line offset based on align-content for multi-line flex
 * @param alignContent Alignment mode
 * @param totalCrossSize Total cross-axis size
 * @param usedCrossSize Used cross-axis size by all lines
 * @param lineCount Number of lines
 * @return Starting offset and spacing between lines
 */
struct LineSpacing {
    f32 startOffset;
    f32 lineGap;
};
LineSpacing ComputeLineSpacing(AlignContent alignContent, f32 totalCrossSize, f32 usedCrossSize, i32 lineCount);

} // namespace Panorama
