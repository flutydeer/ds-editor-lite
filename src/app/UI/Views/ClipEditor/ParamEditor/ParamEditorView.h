//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORVIEW_H
#define PARAMEDITORVIEW_H

#include <QWidget>


class ParamEditorGraphicsView;

class ParamEditorView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorView(QWidget *parent = nullptr);

private:
    ParamEditorGraphicsView *m_graphicsView;
};



#endif // PARAMEDITORVIEW_H
