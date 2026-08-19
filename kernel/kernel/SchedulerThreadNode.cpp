#include <BAN/Assert.h>
#include <BAN/Swap.h>
#include <kernel/SchedulerThreadNode.h>

namespace Kernel
{

	SchedulerThreadNode* SchedulerQueue::front()
	{
		return m_head;
	}

	SchedulerThreadNode* SchedulerQueue::pop_front()
	{
		if (empty())
			return nullptr;
		auto* const result = m_head;
		m_head = m_head->queue.next;
		(m_head ? m_head->queue.prev : m_tail) = nullptr;
		result->queue.prev = nullptr;
		result->queue.next = nullptr;
		return result;
	}

	void SchedulerQueue::push(SchedulerThreadNode* node)
	{
		ASSERT(node->queue.prev == nullptr);
		ASSERT(node->queue.next == nullptr);

		node->queue.prev = m_tail;
		node->queue.next = nullptr;
		(m_tail ? m_tail->queue.next : m_head) = node;
		m_tail = node;
	}

	void SchedulerQueue::pop(SchedulerThreadNode* node)
	{
		(node->queue.prev ? node->queue.prev->queue.next : m_head) = node->queue.next;
		(node->queue.next ? node->queue.next->queue.prev : m_tail) = node->queue.prev;
		node->queue.prev = nullptr;
		node->queue.next = nullptr;
	}

	void SchedulerQueue::walk(void (*callback)(const SchedulerThreadNode*, void*), void* arg) const
	{
		for (auto* node = m_head; node; node = node->queue.next)
			callback(node, arg);
	}



	SchedulerThreadNode* SchedulerHeap::front()
	{
		return m_root;
	}

	SchedulerThreadNode* SchedulerHeap::pop_front()
	{
		if (empty())
			return nullptr;
		auto* const result = m_root;
		pop(result);
		return result;
	}

	void SchedulerHeap::push(SchedulerThreadNode* node)
	{
		ASSERT(node->heap.parent == nullptr);
		ASSERT(node->heap.lchild == nullptr);
		ASSERT(node->heap.rchild == nullptr);

		if (m_root == nullptr)
		{
			// push to empty heap
			node->heap.parent = nullptr;
			node->heap.lchild = nullptr;
			node->heap.rchild = nullptr;
			m_root = node;
			m_last = node;
			return;
		}

		auto* parent = m_last;

		{
			// find parent of the new node
			SchedulerThreadNode* temp;
			while ((temp = parent->heap.parent) && parent == temp->heap.rchild)
				parent = temp;
			if (temp && temp->heap.rchild == nullptr)
				parent = temp;
			else
			{
				if (temp != nullptr)
					parent = temp->heap.rchild;
				while ((temp = parent->heap.lchild))
					parent = temp;
			}
		}

		// insert node as the last node
		(parent->heap.lchild ? parent->heap.rchild : parent->heap.lchild) = node;
		node->heap.parent = parent;
		node->heap.lchild = nullptr;
		node->heap.rchild = nullptr;
		m_last = node;

		// fix heap properties
		while ((parent = node->heap.parent) && node->wake_time_ns < parent->wake_time_ns)
			swap_nodes(node, parent);
	}

	void SchedulerHeap::pop(SchedulerThreadNode* old_node)
	{
		if (m_root == m_last)
		{
			// remove the only node
			old_node->heap.parent = nullptr;
			old_node->heap.lchild = nullptr;
			old_node->heap.rchild = nullptr;
			m_root = nullptr;
			m_last = nullptr;
			return;
		}

		auto* fix_node = m_last;
		swap_nodes(old_node, m_last);

		{
			// update last to point to the previous node
			SchedulerThreadNode* temp;
			while ((temp = m_last->heap.parent) && m_last == temp->heap.lchild)
				m_last = temp;
			if (temp != nullptr)
				m_last = temp->heap.lchild;
			ASSERT(m_last);
			while ((temp = m_last->heap.rchild))
				m_last = temp;
		}

		{
			// delete links to/from the deleted node
			if (auto* parent = old_node->heap.parent)
				(old_node == parent->heap.rchild ? parent->heap.rchild : parent->heap.lchild) = nullptr;
			old_node->heap.parent = nullptr;
			old_node->heap.lchild = nullptr;
			old_node->heap.rchild = nullptr;
		}

		// fix heap properties
		if (fix_node->wake_time_ns == old_node->wake_time_ns)
			;
		else if (fix_node->wake_time_ns < old_node->wake_time_ns)
		{
			SchedulerThreadNode* parent;
			while ((parent = fix_node->heap.parent) && fix_node->wake_time_ns < parent->wake_time_ns)
				swap_nodes(fix_node, parent);
		}
		else for (;;)
		{
			const bool l_ok = !fix_node->heap.lchild || fix_node->wake_time_ns <= fix_node->heap.lchild->wake_time_ns;
			const bool r_ok = !fix_node->heap.rchild || fix_node->wake_time_ns <= fix_node->heap.rchild->wake_time_ns;
			if (l_ok && r_ok)
				break;
			auto* child = (!l_ok && !r_ok)
				? (fix_node->heap.lchild->wake_time_ns < fix_node->heap.rchild->wake_time_ns ? fix_node->heap.lchild : fix_node->heap.rchild)
				: (r_ok ? fix_node->heap.lchild : fix_node->heap.rchild);
			swap_nodes(fix_node, child);
		}
	}

	void SchedulerHeap::walk(void (*callback)(const SchedulerThreadNode*, void*), void* arg) const
	{
		walk_impl(callback, arg, m_root);
	}

	void SchedulerHeap::walk_impl(void (*callback)(const SchedulerThreadNode*, void*), void* arg, const SchedulerThreadNode* node) const
	{
		if (node == nullptr)
			return;
		callback(node, arg);
		walk_impl(callback, arg, node->heap.lchild);
		walk_impl(callback, arg, node->heap.rchild);
	}

	void SchedulerHeap::swap_nodes(SchedulerThreadNode* node1, SchedulerThreadNode* node2)
	{
		if (node1 == node2)
			return;

		if (node2 == node1->heap.parent)
			BAN::swap(node1, node2);

		auto* const p1 = node1->heap.parent;
		auto* const l1 = node1->heap.lchild;
		auto* const r1 = node1->heap.rchild;

		auto* const p2 = node2->heap.parent;
		auto* const l2 = node2->heap.lchild;
		auto* const r2 = node2->heap.rchild;

		if (node1 == node2->heap.parent)
		{
			node1->heap.parent = node2;
			node1->heap.lchild = l2;
			node1->heap.rchild = r2;

			node2->heap.parent = p1;

			if (l1 == node2)
			{
				node2->heap.lchild = node1;
				node2->heap.rchild = r1;
				if (r1) r1->heap.parent = node2;
			}
			else
			{
				node2->heap.lchild = l1;
				node2->heap.rchild = node1;
				if (l1) l1->heap.parent = node2;
			}

			if (p1) (node1 == p1->heap.lchild ? p1->heap.lchild : p1->heap.rchild) = node2;

			if (l2) l2->heap.parent = node1;
			if (r2) r2->heap.parent = node1;
		}
		else
		{
			node1->heap.parent = p2;
			node1->heap.lchild = l2;
			node1->heap.rchild = r2;

			node2->heap.parent = p1;
			node2->heap.lchild = l1;
			node2->heap.rchild = r1;

			if (l1) l1->heap.parent = node2;
			if (r1) r1->heap.parent = node2;

			if (l2) l2->heap.parent = node1;
			if (r2) r2->heap.parent = node1;

			if (p1 || p2)
			{
				if (p1 == p2)
					BAN::swap(p1->heap.lchild, p1->heap.rchild);
				else
				{
					if (p1) (p1->heap.lchild == node1 ? p1->heap.lchild : p1->heap.rchild) = node2;
					if (p2) (p2->heap.lchild == node2 ? p2->heap.lchild : p2->heap.rchild) = node1;
				}
			}
		}

		auto* const root = m_root;
		auto* const last = m_last;

		if      (node1 == root) m_root = node2;
		else if (node1 == last) m_last = node2;

		if      (node2 == root) m_root = node1;
		else if (node2 == last) m_last = node1;
	}

}
