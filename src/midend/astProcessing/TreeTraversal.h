#ifndef TREETRAVERSAL_H
#define TREETRAVERSAL_H

#include <vector>

template <typename TreeNode> class TreeTraversal {
public:
  TreeTraversal();
  virtual ~TreeTraversal();
  virtual void traverse(TreeNode node) = 0;
  virtual bool isNullNode(TreeNode node) { return node == 0; }
  virtual void nullNodeVisit() {}
  virtual bool skipNode(TreeNode node) { return false; }
  virtual bool skipSubTreeOfNode(TreeNode node) { return false; }

protected:
  typedef typename std::vector<TreeNode> ChildrenContainer;
  virtual void setChildrenContainer(TreeNode node, ChildrenContainer &c) = 0;
};

template <typename TreeNode>
class PreOrderTraversal : public TreeTraversal<TreeNode> {
public:
  void traverse(TreeNode root);

protected:
  virtual void preOrderVisit(TreeNode node) = 0;
};

template <typename TreeNode>
class PrePostOrderTraversal : public TreeTraversal<TreeNode> {
protected:
  virtual void preOrderVisit(TreeNode node) = 0;
  virtual void postOrderVisit(TreeNode node) = 0;

public:
  void traverse(TreeNode node);
};

template <class TreeNode> TreeTraversal<TreeNode>::TreeTraversal() {}

template <class TreeNode> TreeTraversal<TreeNode>::~TreeTraversal() {}

template <class TreeNode>
void PreOrderTraversal<TreeNode>::traverse(TreeNode node) {
  typename PreOrderTraversal<TreeNode>::ChildrenContainer c;
  if (this->isNullNode(node)) {
    if (!this->skipNode(node)) {
      this->nullNodeVisit();
    }
    return;
  } else {
    if (!this->skipNode(node)) {
      preOrderVisit(node);
    }
  }
  if (!this->skipSubTreeOfNode(node)) {
    this->setChildrenContainer(node, c);
    for (typename PreOrderTraversal<TreeNode>::ChildrenContainer::iterator
             iter = c.begin();
         iter != c.end(); iter++) {
      traverse(*iter);
    }
  }
}

template <class TreeNode>
void PrePostOrderTraversal<TreeNode>::traverse(TreeNode node) {
  typename PrePostOrderTraversal<TreeNode>::ChildrenContainer c;
  if (this->isNullNode(node)) {
    if (!this->skipNode(node)) {
      this->nullNodeVisit();
    }
    return;
  } else {
    if (!this->skipNode(node)) {
      preOrderVisit(node);
    }
  }
  if (!this->skipSubTreeOfNode(node)) {
    this->setChildrenContainer(node, c);
    for (typename PrePostOrderTraversal<TreeNode>::ChildrenContainer::iterator
             iter = c.begin();
         iter != c.end(); iter++) {
      traverse(*iter);
    }
  }
  if (!this->skipNode(node)) {
    postOrderVisit(node);
  }
}

#endif
