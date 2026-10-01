#ifndef EDITORRESIZEUTILS_H
#define EDITORRESIZEUTILS_H

namespace EditorResizeUtils {
    enum class HorizontalEdge { None, Left, Right };

    // The grab zone may reach outside the rect by outsideExpansion, which is how
    // an affordance drawn beyond the edge (the piano roll note resize handles)
    // makes its drawing the grab target. The default of 0 keeps every caller
    // that only knows the rect itself on exactly the old behaviour.
    inline HorizontalEdge horizontalEdgeAt(const double position, const double width,
                                           const double tolerance,
                                           const double outsideExpansion = 0.0) {
        if (position >= -outsideExpansion && position <= tolerance)
            return HorizontalEdge::Left;
        if (position >= width - tolerance && position <= width + outsideExpansion)
            return HorizontalEdge::Right;
        return HorizontalEdge::None;
    }
}

#endif // EDITORRESIZEUTILS_H
