#ifndef DS_EDITOR_LITE_MACTION_H
#define DS_EDITOR_LITE_MACTION_H

class MAction {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual ~MAction() = default;
};

#endif // DS_EDITOR_LITE_MACTION_H
