#include <iostream>
#include <typeinfo>
#include <thread>
#include "TimeCheck.h"
#include <mutex>
#include <vector>
#include <atomic>

#define NUM_TEST 4000000
#define KEY_RANGE 1000;

using namespace std;

static const int MAX_HEIGHT = 10;

class NODE {
public:
	int key;
	NODE *next[MAX_HEIGHT];
	int toplevel; // index of last link

	NODE() {
		for (int i = 0; i < MAX_HEIGHT; ++i) { next[i] = nullptr; }
		toplevel = MAX_HEIGHT - 1;
	}
	NODE(int value) :key{ value } {
		for (int i = 0; i < MAX_HEIGHT; ++i) { next[i] = nullptr; }
		toplevel = MAX_HEIGHT - 1;
	}
	NODE(int value, int max_index) : key{ value } {
		for (int i = 0; i < max_index; ++i) { next[i] = nullptr; }
		toplevel = max_index - 1;
	}
	~NODE() {};
};

class Virtual_Class {
public:
	Virtual_Class() {};
	virtual ~Virtual_Class() {};

	void Benchmark(int num_thread) {
		int key;
		for (int i = 0; i < NUM_TEST / num_thread; ++i) {
			switch (rand() % 3)
			{
			case 0: key = rand() % KEY_RANGE; Add(key); break;
			case 1: key = rand() % KEY_RANGE; Remove(key); break;
			case 2: key = rand() % KEY_RANGE; Contains(key); break;
			default: cout << "ERROR\n"; exit(-1); break;
			}
		}
	}

	virtual bool Add(int) = 0;
	virtual bool Remove(int) = 0;
	virtual bool Contains(int) = 0;

	virtual void Clear() = 0;
	virtual void Print20() = 0;

	virtual void myTypePrint() = 0;
};

class Coarse_grained_Skip_LIST : public Virtual_Class {
	NODE head, tail;
	mutex m_lock;
public:
	Coarse_grained_Skip_LIST() {
		head.key = 0x80000000;
		for (int i = 0; i < MAX_HEIGHT; ++i) { head.next[i] = &tail; }
		tail.key = 0x7fffffff;
		for (int i = 0; i < MAX_HEIGHT; ++i) { tail.next[i] = nullptr; }
	}
	~Coarse_grained_Skip_LIST() { Clear(); }

	virtual void myTypePrint() { printf(" %s == ���� SkipList\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		NODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];

		m_lock.lock();
		Find(x, prev, curr);

		if (x == curr[0]->key) {
			m_lock.unlock();
			return false;
		}
		else {
			int height = rand() % MAX_HEIGHT; // ���̿� ���� �� �� ������ �ʿ�
			NODE *node = new NODE{ x, height };
			//node->next = curr;	// �����ؾߵ�
			//prev->next = node;
			m_lock.unlock();
			return true;
		}
	};

	virtual bool Remove(int x) {
		NODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];

		m_lock.lock();
		Find(x, prev, curr);

		if (x != curr[0]->key) {
			m_lock.unlock();
			return false;
		}
		else {
			//prev->next = curr->next;
			delete curr;
			m_lock.unlock();
			return true;
		}
	};

	virtual bool Contains(int x) {
		NODE *prev[MAX_HEIGHT], *curr[MAX_HEIGHT];

		m_lock.lock();
		Find(x, prev, curr);

		if (x != curr[0]->key) {
			m_lock.unlock();
			return false;
		}
		else {
			m_lock.unlock();
			return true;
		}
	};

	virtual void Clear() {
		while (head.next[0] != &tail)
		{
			NODE * ptr = head.next[0];
			head.next[0] = ptr->next[0];
			delete ptr;
		}

		for (int i = 0; i < MAX_HEIGHT; ++i) { head.next[i] = &tail; }
	}

	virtual void Print20() {
		NODE *ptr = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next[0];
		}
		cout << "\n\n";
	}
private:

	void Find(int key, NODE *prev[], NODE *curr[]) {
		prev[MAX_HEIGHT - 1] = &head;
		curr[MAX_HEIGHT - 1] = head.next[MAX_HEIGHT];
		for (int level = MAX_HEIGHT - 1; level >= 0; --level) {
			while (curr[level]->key < key)
			{
				prev[level] = curr[level];
				curr[level] = curr[level]->next[level];
			}
			if (0 == level) break;
			prev[level - 1] = prev[level];
			curr[level - 1] = prev[level - 1]->next[level - 1];
		}
	}
};
//
//class Fine_grained_synchronization_LIST : public Virtual_Class {
//	NODE head, tail;
//public:
//	Fine_grained_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
//	~Fine_grained_synchronization_LIST() {
//		// head.m_L.try_lock.lock();
//
//		Clear();
//	}
//
//	virtual void myTypePrint() { printf(" %s == ������ ����ȭ\n\n", typeid(*this).name()); }
//
//	virtual bool Add(int x) {
//		NODE *prev, *curr;
//
//		prev = Search_key(x);
//		curr = prev->next;
//
//		if (x == curr->key) {
//			prev->Unlock();
//			curr->Unlock();
//			return false;
//		}
//		else {
//			NODE *node = new NODE{ x };
//			node->next = curr;
//			prev->next = node;
//			prev->Unlock();
//			curr->Unlock();
//			return true;
//		}
//	};
//
//	virtual bool Remove(int x) {
//		NODE *prev, *curr;
//
//		prev = Search_key(x);
//		curr = prev->next;
//		if (x != curr->key) {
//			prev->Unlock();
//			curr->Unlock();
//			return false;
//		}
//		else {
//			prev->next = curr->next;
//			curr->Unlock();
//			delete curr;
//			prev->Unlock();
//			return true;
//		}
//	};
//
//	// �ش� key ���� �ֽ��ϱ�?
//	virtual bool Contains(int x) {
//		NODE *prev, *curr;
//
//		prev = Search_key(x);
//		curr = prev->next;
//
//		if (x != curr->key) {
//			prev->Unlock();
//			curr->Unlock();
//			return false;
//		}
//		else {
//			prev->Unlock();
//			curr->Unlock();
//			return true;
//		}
//	};
//
//	// ���� ��� ���� ����
//	virtual void Clear() {
//		NODE *ptr;
//		while (head.next != &tail)
//		{
//			ptr = head.next;
//			head.next = ptr->next;
//			delete ptr;
//		}
//	}
//
//	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
//	virtual void Print20() {
//		NODE *ptr = head.next;
//		for (int i = 0; i < 20; ++i) {
//			if (&tail == ptr) { break; }
//			cout << ptr->key << " ";
//			ptr = ptr->next;
//		}
//		cout << "\n\n";
//	}
//private:
//	NODE* Search_key(int key) {
//		NODE *prev, *curr;
//
//		head.Lock();
//		prev = &head;
//
//		head.next->Lock();
//		curr = head.next;
//
//		while (curr->key < key) {
//
//			prev->Unlock();
//			prev = curr;
//
//			curr->next->Lock();
//			curr = curr->next;
//		}
//		return prev;
//	}
//};
//
//class Optimistic_synchronization_LIST : public Virtual_Class {
//	// �˻��� ���� ������尡 �ʹ� ũ��, �ϴ� �˻��� ��. ����� �ߴ��� Ȯ����
//	// �߸��Ǿ�����, ó������ �ٽ� -> ��õ�� ����ȭ�� ���� (�õ��� ��ġ�� ����� ���� �� ����)
//	// ���1. �̵��߿� ���ڱ� ���� �����ع����� ��� �ɰ��ΰ�? delete �� ��带 ����Ų��? ����
//	// ����� �� �����Ͷ�� ������ ���� ������ �� ó������� �Ѵ�.
//	// �׷��� ������, ���� �ϴµ� DELETE �� ���� �ʰ�, �ٸ� ���� ���д�. ( �����ʹ� �״�δ� ������ )
//
//	// ���� �����ؼ�, �ι�° ������ �ٽ� �� �� �ִ��� Ȯ�� �Ѵ�.
//	// ù��° �˻��� �ƹ��� �� ���� �˻�������, ������ ���� ��ġ ���� �ɾ� ���� �ٽ� �˻���
//
//	/********* ������ �޸� ������ �ִٴ� �� *********/
//
//	NODE head, tail;
//public:
//	Optimistic_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
//	~Optimistic_synchronization_LIST() { Clear(); }
//
//	virtual void myTypePrint() { printf(" %s == ��õ�� ����ȭ, �޸𸮸� �������� �����Ƿ� ������ �ȴ�.\n\n", typeid(*this).name()); }
//
//	virtual bool Add(int x) {
//		NODE *prev, *curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->next;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;
//			}
//
//			if (x == curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//				NODE *node = new NODE{ x };
//				node->next = curr;
//				prev->next = node;
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	virtual bool Remove(int x) {
//		NODE *prev, *curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->next;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				prev->Unlock();
//				curr->Unlock();
//				continue;;
//			}
//
//			if (x != curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//
//				NODE *p = free_list.GetNode(curr->key);
//
//				prev->next = curr->next;
//				//free_list.insert(curr);
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	// �ش� key ���� �ֽ��ϱ�?
//	virtual bool Contains(int x) {
//		NODE *prev, *curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->next;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;
//			}
//
//			if (x != curr->key) {
//				prev->Unlock();
//				curr->Unlock();
//				return false;
//			}
//			else {
//				prev->Unlock();
//				curr->Unlock();
//				return true;
//			}
//		}
//	};
//
//	// ���� ��� ���� ����
//	virtual void Clear() {
//		NODE *ptr;
//		while (head.next != &tail)
//		{
//			ptr = head.next;
//			head.next = ptr->next;
//			delete ptr;
//		}
//
//		//free_list.Clear();
//	}
//
//	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
//	virtual void Print20() {
//		NODE *ptr = head.next;
//		for (int i = 0; i < 20; ++i) {
//			if (&tail == ptr) { break; }
//			cout << ptr->key << " ";
//			ptr = ptr->next;
//		}
//		cout << "\n\n";
//	}
//private:
//	NODE* Search_key(int key) {
//		NODE *prev, *curr;
//
//		prev = &head;
//		curr = head.next;
//
//		while (curr->key < key) {
//			prev = curr;
//			curr = curr->next;
//		}
//		return prev;
//	}
//
//	bool validate(NODE *prev, NODE *curr) { return (Search_key(curr->key)->next == curr); }
//};
//
//class Lazy_synchronization_List : public Virtual_Class {
//	// ��õ�� ����ȭ�� ���� -> marked �� ����Ͽ� ���� �������� ���� ����
//
//	// free-list �� ������ �ʾƼ�, ���ó� ����
//
//	NODE head, tail;
//public:
//	Lazy_synchronization_List() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
//	~Lazy_synchronization_List() { Clear(); }
//
//	virtual void myTypePrint() { printf(" %s == ������ ����ȭ, ���ó� free-list �� ������ �ʾƼ� ����\n\n", typeid(*this).name()); }
//
//	virtual bool Add(int x) {
//		NODE *prev, *curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->next;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;
//			}
//
//			if (x == curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//				NODE *node = new NODE{ x };
//				node->next = curr;
//				prev->next = node;
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	virtual bool Remove(int x) {
//		NODE *prev, *curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->next;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;;
//			}
//
//			if (x != curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//				curr->marked = true;
//				prev->next = curr->next;
//				//free_list.insert(curr);
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	virtual bool Contains(int x) {
//		NODE *prev, *curr;
//
//		prev = Search_key(x);
//		curr = prev->next;
//
//		return ((curr->key == x) && (!curr->marked));
//	};
//
//	// ���� ��� ���� ����
//	virtual void Clear() {
//		NODE *ptr;
//		while (head.next != &tail)
//		{
//			ptr = head.next;
//			head.next = ptr->next;
//			delete ptr;
//		}
//
//		//free_list.Clear();
//	}
//
//	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
//	virtual void Print20() {
//		NODE *ptr = head.next;
//		for (int i = 0; i < 20; ++i) {
//			if (&tail == ptr) { break; }
//			cout << ptr->key << " ";
//			ptr = ptr->next;
//		}
//		cout << "\n\n";
//	}
//private:
//	NODE* Search_key(int key) {
//		NODE *prev, *curr;
//
//		prev = &head;
//		curr = head.next;
//
//		while (curr->key < key) {
//			prev = curr;
//			curr = curr->next;
//		}
//		return prev;
//	}
//
//	bool validate(NODE *prev, NODE *curr) {
//		return ((!prev->marked) && (!curr->marked) && (prev->next == curr));
//	}
//};
//
//class Shared_ptr_Lazy_synchronization_List : public Virtual_Class {
//	// shared_ptr
//
//	// �Ϲ� �����Ϳ��� ->next ������ atomic �ϰ� �ѹ��� 32bit ������ ���ư��� ������ �߰����� ����.
//	// ������, shared_ptr ���� ���� ���� �׷��� �ʴ�. �߰����� �����Ѵ�. 2 stack ���� �а� ����. ( 8 bytes )
//	// �߰��� �����Ͱ� �����ϱ� ������, �а� ���� �߰��� ������ �߻��� �� �ۿ� ����.
//
//	// �ذ�å 1. lock �� �Ǵ� ! (������ ���� �˸���ũ)
//	// �ذ�å 2. atomic_shared_ptr �� ����. ( ������ �̷��� ����. �ٸ� C++20 ǥ�� ���� ) -> �Ĵ� ���� �ִ�. ���ɵ� ������!
//	// �ذ�å 3. atomic �� shared_ptr �� �����.
//	// �ذ�å 4. atomic operation ( atomic stom, atomic load ���� �Լ��� ���� �ִ�. �۵��� atomic �ϰ�.. )
//	shared_ptr<NODE> head, tail;
//public:
//	Shared_ptr_Lazy_synchronization_List() { head = make_shared<NODE>(0x80000000, 0); tail = make_shared<NODE>(0x7fffffff, 0); head->shared = tail; tail->shared = nullptr; }
//	~Shared_ptr_Lazy_synchronization_List() { Clear(); head = nullptr; tail = nullptr; }
//
//	virtual void myTypePrint() { printf(" %s == ������ ����ȭ, shared_ptr ( �����Ͱ� ����� �߰����� Ƣ����Ƿ� ���� )\n\n", typeid(*this).name()); }
//
//	virtual bool Add(int x) {
//		shared_ptr<NODE> prev, curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->shared;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;
//			}
//
//			if (x == curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//				shared_ptr<NODE> node = make_shared<NODE>(x, 0);
//				node->shared = curr;
//				prev->shared = node;
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	virtual bool Remove(int x) {
//		shared_ptr<NODE> prev, curr;
//
//		while (true)
//		{
//			prev = Search_key(x);
//			curr = prev->shared;
//
//			prev->Lock();
//			curr->Lock();
//
//			if (false == validate(prev, curr)) {
//				curr->Unlock();
//				prev->Unlock();
//				continue;
//			}
//
//			if (x != curr->key) {
//				curr->Unlock();
//				prev->Unlock();
//				return false;
//			}
//			else {
//				curr->marked = true;
//				prev->shared = curr->shared;
//				//free_list.insert(curr);
//				curr->Unlock();
//				prev->Unlock();
//				return true;
//			}
//		}
//	};
//
//	virtual bool Contains(int x) {
//		shared_ptr<NODE> prev, curr;
//
//		prev = Search_key(x);
//		curr = prev->shared;
//
//		return ((curr->key == x) && (!curr->marked));
//	};
//
//	// ���� ��� ���� ����
//	virtual void Clear() {
//		head->shared = tail;
//	}
//
//	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
//	virtual void Print20() {
//		shared_ptr<NODE> ptr = head->shared;
//		for (int i = 0; i < 20; ++i) {
//			if (tail == ptr) { break; }
//			cout << ptr->key << " ";
//			ptr = ptr->shared;
//		}
//		cout << "\n\n";
//	}
//private:
//	shared_ptr<NODE> Search_key(int key) {
//		shared_ptr<NODE> prev, curr;
//
//		prev = head;
//		curr = head->shared;
//
//		while (curr->key < key) {
//			// ������ ������ �ذ��Ϸ���, �� ��帶�� lock �� �ɾ�� �ϴµ�...
//			// �׷��� �Ǹ� ��ǻ� ������ ����ȭ�� �ٸ��ٰ� ����.
//
//			// C++ ���� atomic_store �� ���� �ڵ� ��ȯ ������ �������� �����Ƿ�, ���� Node �� �����ؾ���
//			/// *(reinterpret_cast<long long *>(prev)) atomic_store( reinterpret_cast<long long>(curr) );
//			prev = curr;
//			curr = curr->shared;
//		}
//		return prev;
//	}
//
//	bool validate(const shared_ptr<NODE> &prev, const shared_ptr<NODE> &curr) {
//		return ((!prev->marked) && (!curr->marked) && (prev->shared == curr));
//	}
//};
//
//using LFNODE = class Lock_free_Node {
//public:
//	unsigned int next;
//	int key;
//
//	Lock_free_Node(int x) : key{ x } { next = NULL; }
//	Lock_free_Node() { next = NULL; }
//	~Lock_free_Node() {};
//
//	bool DoMark(Lock_free_Node *ptr) { return CAS(ptr, ptr, false, true); }
//
//	Lock_free_Node *GetNext() { return reinterpret_cast<Lock_free_Node*>(next & 0xFFFFFFFE); }
//
//	Lock_free_Node *GetNextWithMark(bool *removed) {
//		*removed = (1 == (next & 0x1));
//		return reinterpret_cast<Lock_free_Node*>(next & 0xFFFFFFFE);
//	}
//
//	bool CAS(int old_value, int new_value) {
//		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int *>(&next), &old_value, new_value);
//	}
//
//	bool CAS(Lock_free_Node *old_aadr, Lock_free_Node *new_addr, bool old_mark, bool new_mark) {
//		int old_value = reinterpret_cast<int>(old_aadr);
//		if (old_mark) { old_value = old_value | 1; }
//		else { old_value = old_value & 0xFFFFFFFE; }
//
//		int new_value = reinterpret_cast<int>(new_addr);
//		if (new_mark) { new_value = new_value | 1; }
//		else { new_value = new_value & 0xFFFFFFFE; }
//
//		return CAS(old_value, new_value);
//	}
//};
//
//class Nonblocking_synchronization_List : public Virtual_Class {	// �ݿ��� ( ��ȣ )
//	LFNODE head, tail;
//public:
//	Nonblocking_synchronization_List() {
//		head.key = 0x80000000;
//		head.next = reinterpret_cast<int>(&tail);
//		tail.key = 0x7fffffff;
//		tail.next = NULL;
//	}
//	~Nonblocking_synchronization_List() { Clear(); }
//
//	virtual void myTypePrint() { printf(" %s == ����� ����ȭ\n\n", typeid(*this).name()); }
//
//	virtual bool Add(int x) {
//		LFNODE *prev, *curr;
//		bool removed;
//
//	RESTART:
//
//		prev = &head;
//		curr = prev->GetNext();
//
//		while (true)
//		{
//			//prev = Search_key(x, &prev, &curr);
//			Search_key(x, &prev, &curr);
//
//			if (x == curr->key) {
//				return false;
//			}
//			else {
//				LFNODE *node = new LFNODE{ x };
//				node->next = reinterpret_cast<unsigned int>(curr);
//				//prev->next = reinterpret_cast<unsigned int>(node);
//				if (true == prev->CAS(curr, node, false, false)) { return true; }
//				//return true;
//				goto RESTART;
//			}
//		}
//	};
//
//	virtual bool Remove(int x) {
//		LFNODE *prev, *curr;
//
//		prev = &head;
//		curr = prev->GetNext();
//
//		while (true)
//		{
//			Search_key(x, &prev, &curr);
//
//			if (x != curr->key) {
//				return false;
//			}
//			else {
//				LFNODE *succ = curr->GetNext();
//				if (true == curr->DoMark(succ)) {
//
//					if (false == prev->CAS(curr, succ, false, false)) {
//						return false;
//					}
//
//					return true;
//				}
//				return true;
//			}
//		}
//	};
//
//	virtual bool Contains(int x) {
//		LFNODE *prev, *curr;
//
//		prev = &head;
//		curr = prev->GetNext();
//		while (prev->key >= x)
//		{
//			prev = curr;
//			curr = curr->GetNext();
//		}
//
//		return ((prev->key == x) && (!(prev->next) & 0x01));
//	};
//
//	// ���� ��� ���� ����
//	virtual void Clear() {
//
//		while (head.next != reinterpret_cast<int>(&tail))
//		{
//			LFNODE *ptr = reinterpret_cast<LFNODE *>(head.next);
//			head.next = reinterpret_cast<int>(ptr->GetNext());
//			delete ptr;
//		}
//	}
//
//	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
//	virtual void Print20() {
//		LFNODE *ptr = head.GetNext();
//		for (int i = 0; i < 20; ++i) {
//			if (&tail == ptr) { break; }
//			cout << ptr->key << " ";
//			ptr = ptr->GetNext();
//		}
//		cout << "\n\n";
//	}
//private:
//	LFNODE * Search_key(int key, LFNODE** prev, LFNODE** curr) {
//
//		LFNODE *pr = *prev, *cu = *curr;
//		bool marked;
//
//	RESTART:
//		pr = &head, cu = pr->GetNext();
//
//		while (true)
//		{
//			LFNODE *succ = cu->GetNextWithMark(&marked);
//			while (true == marked)
//			{
//				// ���⼭ ������ 2�� �̻� �� ��, ���ѷ��� �ɸ�... ??
//				if (false == pr->CAS(cu, succ, false, false)) { goto RESTART; }
//				cu = succ;
//				succ = cu->GetNextWithMark(&marked);
//			}
//			if (cu->key >= key) { return pr; }
//			pr = cu;
//			cu = succ;
//		}
//	}
//};


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	List_Classes.emplace_back(new Coarse_grained_Skip_LIST());
	//List_Classes.emplace_back(new Fine_grained_synchronization_LIST());
	//List_Classes.emplace_back(new Optimistic_synchronization_LIST());	// �޸� ������ �ִ�.
	//List_Classes.emplace_back(new Lazy_synchronization_List());	// �޸� ������ �ִ�.
	//List_Classes.emplace_back(new Shared_ptr_Lazy_synchronization_List());	// ���ٰ� ����
	//List_Classes.emplace_back(new Nonblocking_synchronization_List());

	vector<thread *> worker_thread;
	Time_Check t;

	for (auto classes : List_Classes) {
		classes->myTypePrint();
		for (int num_thread = 1; num_thread <= 16; num_thread *= 2) {
			t.check();
			for (int i = 0; i < num_thread; ++i) { worker_thread.push_back(new thread{ &Virtual_Class::Benchmark, classes, num_thread }); }
			for (auto p_th : worker_thread) { p_th->join(); }
			t.check_end();
			for (auto p_th : worker_thread) { delete p_th; }
			worker_thread.clear();

			cout << num_thread << " Core\t";
			t.show();
			classes->Print20();
			classes->Clear();
		}
		cout << "\n---- Next Class ----\n";
	}

	for (auto classes : List_Classes) { delete classes; }
	system("pause");
}