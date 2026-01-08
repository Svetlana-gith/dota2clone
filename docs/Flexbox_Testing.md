# Flexbox Layout Testing Guide

## Test Scenarios

### ✅ Basic Tests (Completed)

#### 1. Simple Column Layout
```css
#Container {
    display: flex;
    flex-direction: column;
    gap: 16px;
}
```
**Expected**: Children stacked vertically with 16px gaps
**Status**: ✅ Working

#### 2. Simple Row Layout
```css
#Container {
    display: flex;
    flex-direction: row;
    gap: 16px;
}
```
**Expected**: Children arranged horizontally with 16px gaps
**Status**: ✅ Working

#### 3. Center Alignment
```css
#Container {
    display: flex;
    justify-content: center;
    align-items: center;
}
```
**Expected**: Children centered both horizontally and vertically
**Status**: ✅ Working

#### 4. Space Between
```css
#Container {
    display: flex;
    justify-content: space-between;
}
```
**Expected**: First child at start, last at end, equal spacing between
**Status**: ✅ Working

#### 5. Flex Grow
```css
.Child {
    flex-grow: 1;
}
```
**Expected**: Children expand to fill available space proportionally
**Status**: ✅ Working

#### 6. Flex Shrink
```css
.Child {
    flex-shrink: 0;
}
```
**Expected**: Children don't shrink when container is too small
**Status**: ✅ Working (fixed to work even with flex-grow)

### 🔄 Advanced Tests (To Implement)

#### 7. Nested Flex Containers
```css
#Outer {
    display: flex;
    flex-direction: column;
}

#Inner {
    display: flex;
    flex-direction: row;
}
```
**Expected**: Nested flex layouts work correctly
**Status**: ⏳ Needs testing

#### 8. Mixed Flex and Flow
```css
#FlexContainer {
    display: flex;
}

#FlowContainer {
    flow-children: down;
}
```
**Expected**: Flex and traditional flow layouts coexist
**Status**: ⏳ Needs testing

#### 9. Stretch Alignment
```css
#Container {
    display: flex;
    align-items: stretch;
}
```
**Expected**: Children stretch to fill cross-axis
**Status**: ✅ Working (basic implementation)
**TODO**: Account for margins

#### 10. Complex Login Form
See `resources/styles/login-flexbox.css`
**Expected**: Login form with proper spacing and alignment
**Status**: ⏳ Needs visual testing

## Future Features

### Flex Wrap (Priority: Medium)
```css
#Container {
    display: flex;
    flex-wrap: wrap;
}
```
**Use case**: Responsive grids, tag lists, toolbars
**Implementation**: 
- Add `FlexWrap` enum (NoWrap, Wrap, WrapReverse)
- Track multiple lines in LayoutFlex()
- Implement line breaking logic

### Align Content (Priority: Medium)
```css
#Container {
    display: flex;
    flex-wrap: wrap;
    align-content: space-between;
}
```
**Use case**: Multi-line flex containers
**Requires**: flex-wrap implementation first

### Order Property (Priority: Low)
```css
.Child {
    order: 2;
}
```
**Use case**: Reordering elements without changing DOM
**Implementation**: Sort items by order before layout

### Margin Support in Cross-Axis (Priority: High)
```css
.Child {
    margin-top: 10px;
    margin-bottom: 10px;
}
```
**Current**: Margins ignored in cross-axis alignment
**TODO**: Account for margins in AlignCross()

### Separate Measure Pass (Priority: Medium)
**Current**: PerformLayout() called twice (measure + final)
**Optimization**: Add Measure() method to avoid full layout
**Benefit**: ~30-40% performance improvement for complex layouts

### Debug Overlay (Priority: High)
```cpp
#ifdef UI_DEBUG_LAYOUT
    DrawFlexDebugOverlay(parent);
#endif
```
**Features**:
- Draw container bounds (red)
- Draw item bounds (green)
- Show main/cross axis lines
- Display flex properties (grow, shrink, basis)
- Show computed sizes

## Testing Checklist

### Visual Tests
- [ ] Login screen with flexbox CSS
- [ ] Register screen with flexbox CSS
- [ ] Nested flex containers (header with logo + buttons)
- [ ] Horizontal toolbar with space-between
- [ ] Vertical list with gaps
- [ ] Mixed flex-grow items
- [ ] Stretch alignment with different sized children

### Edge Cases
- [ ] Empty container (no children)
- [ ] Single child
- [ ] Container smaller than content (overflow)
- [ ] Negative remaining space
- [ ] Zero gap
- [ ] All children with flex-grow: 0
- [ ] All children with flex-shrink: 0

### Performance Tests
- [ ] 100+ items in flex container
- [ ] Deeply nested flex (5+ levels)
- [ ] Rapid layout invalidation (resize, add/remove children)
- [ ] Compare flex vs flow performance

## Known Issues

### Fixed
- ✅ Shrink not working when flex-grow > 0
- ✅ Negative sizes when container too small

### Open
- ⚠️ Margins not accounted in cross-axis alignment
- ⚠️ Double layout pass (measure + final)
- ⚠️ No visual debugging tools

## Implementation Notes

### Current Architecture
```
LayoutFlex()
├── Phase 1: Measure
│   └── child->PerformLayout(contentBounds)  // Get natural size
├── Phase 2: Grow/Shrink
│   ├── Distribute remaining space (grow)
│   └── Shrink items if overflow
└── Phase 3: Position
    ├── Calculate cursor based on justify-content
    ├── Position each item
    └── Apply cross-axis alignment
```

### Performance Characteristics
- **Time Complexity**: O(n) where n = number of children
- **Space Complexity**: O(n) for FlexItem array
- **Layout Passes**: 2 (measure + final)

### Memory Usage
- FlexItem struct: ~32 bytes per child
- Typical form (10 children): ~320 bytes
- Complex UI (100 children): ~3.2 KB

## Next Steps

1. **Test login-flexbox.css** - Visual verification
2. **Implement debug overlay** - Essential for debugging
3. **Add margin support** - Fix cross-axis alignment
4. **Optimize measure pass** - Performance improvement
5. **Add flex-wrap** - Enable responsive layouts
