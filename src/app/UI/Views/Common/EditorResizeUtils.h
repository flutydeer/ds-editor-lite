#ifndef EDITORRESIZEUTILS_H
#define EDITORRESIZEUTILS_H

namespace EditorResizeUtils {
    enum class HorizontalEdge { None, Left, Right };

    inline HorizontalEdge horizontalEdgeAt(const double position, const double width,
                                           const double tolerance) {
        if (position >= 0.0 && position <= tolerance)
            return HorizontalEdge::Left;
        if (position >= width - tolerance && position <= width)
            return HorizontalEdge::Right;
        return HorizontalEdge::None;
    }
}

#endif // EDITORRESIZEUTILS_H
