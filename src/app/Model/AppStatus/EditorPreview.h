#ifndef EDITORPREVIEW_H
#define EDITORPREVIEW_H

namespace EditorPreview {
    struct Note {
        int id = -1;
        int rStart = 0;
        int length = 0;
        int keyIndex = 0;

        friend bool operator==(const Note &lhs, const Note &rhs) = default;
    };
}

#endif // EDITORPREVIEW_H
