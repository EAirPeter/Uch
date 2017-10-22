#pragma once

#include "Common.hpp"

#include "Pool.hpp"

template<class tElem, class tElemPool>
class IntrList;

template<class tElem>
class IntrListNode {
    template<class tElem, class tElemPool>
    friend class IntrList;

    static_assert(!std::is_const_v<tElem>, "const");

public:
    using Element = tElem;

public:
    inline IntrListNode() = default;

    IntrListNode(const IntrListNode &) = delete;
    IntrListNode(IntrListNode &&) = delete;

    IntrListNode &operator =(const IntrListNode &) = delete;
    IntrListNode &operator =(IntrListNode &&vNode) = delete;

public:
    constexpr Element *GetPrev() noexcept {
        return static_cast<Element *>(x_pPrev);
    }

    constexpr Element *GetNext() noexcept {
        return static_cast<Element *>(x_pNext);
    }

    constexpr const Element *GetPrev() const noexcept {
        return static_cast<const Element *>(x_pPrev);
    }

    constexpr const Element *GetNext() const noexcept {
        return static_cast<const Element *>(x_pNext);
    }

private:
    constexpr IntrListNode(IntrListNode *pPrev, IntrListNode *pNext) noexcept : x_pPrev(pPrev), x_pNext(pNext) {}

    static constexpr void X_Link(IntrListNode *pLhs, IntrListNode *pRhs) {
        pLhs->x_pNext = pRhs;
        pRhs->x_pPrev = pLhs;
    }

private:
    IntrListNode *x_pPrev;
    IntrListNode *x_pNext;

};

template<class tElem, class tElemPool = SysPool<tElem>>
class IntrList {
public:
    using Element = tElem;
    using ElemPool = tElemPool;
    using UpElem = typename ElemPool::UniquePtr;

protected:
    using Node = IntrListNode<Element>;

    static_assert(std::is_base_of_v<Node, Element>, "derivation");

public:
    constexpr IntrList(ElemPool &vPool) noexcept : x_pPool(&vPool) {}

    IntrList() = delete;
    IntrList(const IntrList &) = delete;

    constexpr IntrList(IntrList &&vList) noexcept {
        vList.Swap(*this);
    }

    ~IntrList() {
        auto pNode = X_Head();
        while (pNode != X_Nil())
            x_pPool->Delete(static_cast<Element *>(std::exchange(pNode, pNode->x_pNext)));
    }

    IntrList &operator =(const IntrList &) = delete;
    
    constexpr IntrList &operator =(IntrList &&vList) noexcept {
        vList.Swap(*this);
        return *this;
    }

    constexpr void Swap(IntrList &vList) noexcept {
        auto pHead = X_Head();
        auto pTail = X_Tail();
        if (vList.IsEmpty())
            Node::X_Link(X_Nil(), X_Nil());
        else {
            Node::X_Link(X_Nil(), vList.X_Head());
            Node::X_Link(vList.X_Tail(), X_Nil());
        }
        if (pHead == X_Nil())
            Node::X_Link(vList.X_Nil(), vList.X_Nil());
        else {
            Node::X_Link(vList.X_Nil(), pHead);
            Node::X_Link(pTail, vList.X_Nil());
        }
        using std::swap;
        swap(x_pPool, vList.x_pPool);
    }

    friend constexpr void swap(IntrList &vLhs, IntrList &vRhs) noexcept {
        vRhs.Swap(vLhs);
    }

public:
    constexpr ElemPool &GetElemPool() const noexcept {
        return *const_cast<ElemPool *>(x_pPool);
    }

    constexpr bool IsEmpty() const noexcept {
        return X_Head() == X_Nil();
    }

    constexpr U32 GetSize() const noexcept {
        U32 uRes = 0;
        for (auto pNode = X_Head(); pNode != X_Nil(); pNode = pNode->x_pNext)
            ++uRes;
        return uRes;
    }

    // unchecked
    constexpr Element *GetHead() noexcept {
        return static_cast<Element *>(X_Head());
    }

    // unchecked
    constexpr Element *GetTail() noexcept {
        return static_cast<Element *>(X_Tail());
    }

    constexpr Element *GetNil() noexcept {
        return static_cast<Element *>(X_Nil());
    }

    // unchecked
    constexpr const Element *GetHead() const noexcept {
        return static_cast<const Element *>(X_Head());
    }

    // unchecked
    constexpr const Element *GetTail() const noexcept {
        return static_cast<const Element *>(X_Tail());
    }

    constexpr const Element *GetNil() const noexcept {
        return static_cast<const Element *>(X_Nil());
    }

public:
    constexpr void Clear() noexcept {
        auto pNode = X_Head();
        while (pNode != X_Nil())
            x_pPool->Delete(static_cast<Element *>(std::exchange(pNode, pNode->x_pNext)));
        Node::X_Link(X_Nil(), X_Nil());
    }

    constexpr void PushHead(UpElem upElem) noexcept {
        InsertAfter(GetNil(), std::move(upElem));
    }

    constexpr void PushTail(UpElem upElem) noexcept {
        InsertBefore(GetNil(), std::move(upElem));
    }

    // unchecked
    constexpr UpElem PopHead() noexcept {
        return Remove(GetHead());
    }

    // unchecked
    constexpr UpElem PopTail() noexcept {
        return Remove(GetTail());
    }

    constexpr static void InsertAfter(Element *pHint, UpElem upNew) noexcept {
        auto pNew = upNew.release();
        Node::X_Link(pNew, pHint->Node::x_pNext);
        Node::X_Link(pHint, pNew);
    }

    constexpr static void InsertBefore(Element *pHint, UpElem upNew) noexcept {
        auto pNew = upNew.release();
        Node::X_Link(pHint->Node::x_pPrev, pNew);
        Node::X_Link(pNew, pHint);
    }

    constexpr UpElem Remove(Element *pElem) noexcept {
        Node::X_Link(pElem->Node::x_pPrev, pElem->Node::x_pNext);
        return x_pPool->Wrap(pElem);
    }

public:
    constexpr void Splice(IntrList &&vList) noexcept {
        Splice(vList);
    }

    constexpr void Splice(IntrList &vList) noexcept {
        if (vList.IsEmpty())
            return;
        Node::X_Link(X_Tail(), vList.X_Head());
        Node::X_Link(vList.X_Tail(), X_Nil());
        Node::X_Link(vList.X_Nil(), vList.X_Nil());
    }

    constexpr IntrList ExtractFromHeadTo(Element *pTail) noexcept {
        auto pHead = GetHead();
        Node::X_Link(X_Nil(), pTail->Node::x_pNext);
        return {pHead, pTail, x_pPool};
    }

    constexpr IntrList ExtractToTailFrom(Element *pHead) noexcept {
        auto pTail = GetTail();
        Node::X_Link(pHead->Node::x_pPrev, X_Nil());
        return {pHead, pTail, x_pPool};
    }

private:
    constexpr IntrList(Element *pHead, Element *pTail, ElemPool *pPool) noexcept :
        x_vNil(pTail, pHead), x_pPool(pPool)
    {
        pHead->Node::x_pPrev = &x_vNil;
        pTail->Node::x_pNext = &x_vNil;
    }

    constexpr Node *X_Head() noexcept {
        return X_Nil()->x_pNext;
    }

    constexpr const Node *X_Head() const noexcept {
        return X_Nil()->x_pNext;
    }

    constexpr Node *X_Tail() noexcept {
        return X_Nil()->x_pPrev;
    }

    constexpr const Node *X_Tail() const noexcept {
        return X_Nil()->x_pPrev;
    }

    constexpr Node *X_Nil() noexcept {
        return &x_vNil;
    }

    constexpr const Node *X_Nil() const noexcept {
        return &x_vNil;
    }

private:
    Node x_vNil {&x_vNil, &x_vNil};
    ElemPool *x_pPool;

};
