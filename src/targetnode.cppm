
export module targetnode;

class TargetNode {
public:
    TargetNode *parent() {
        return _parent;
    }

private:
    TargetNode *_parent;
};
