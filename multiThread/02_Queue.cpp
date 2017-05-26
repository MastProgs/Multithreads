#include <iostream>
#include <typeinfo>
#include <thread>
#include "TimeCheck.h"
#include <mutex>
#include <vector>
#include <atomic>
#include <Windows.h>

#define NUM_TEST 10000000
#define KEY_RANGE 1000

using namespace std;
using namespace chrono;

class Virtual_Class {
public:
	Virtual_Class() {};
	virtual ~Virtual_Class() {};

	void Benchmark(int num_thread) {
		for (int i = 0; i < NUM_TEST / num_thread; ++i) {
			if ((1 & rand()) || (i < KEY_RANGE / num_thread)) { Enqueue(i); }
			else { Dequeue(); }
		}
	}

	virtual void Enqueue(int) = 0;
	virtual int Dequeue() = 0;

	virtual void Clear() = 0;
	virtual void Print20() = 0;

	virtual void myTypePrint() = 0;
};

class NODE
{
public:
	int key;
	NODE * next = nullptr;

	NODE() { next = nullptr; };
	NODE(int k) : key{ k }, next{ nullptr } {};
	~NODE() {};
};

class CorseGrain_QUEUE : public Virtual_Class
{
	NODE *head;
	NODE *tail;

	mutex enqL;
	mutex deqL;
public:

	CorseGrain_QUEUE() {
		tail = head = new NODE;
		tail->next = nullptr;
	};
	~CorseGrain_QUEUE() {
		Clear();
		if (nullptr != head) { delete head; }
	};

private:
	virtual void Enqueue(int key) {
		enqL.lock();
		tail->next = new NODE{ key };
		tail = tail->next;
		enqL.unlock();
	};

	virtual int Dequeue() {
		deqL.lock();
		if (nullptr == head->next) {
			deqL.unlock();
			cout << "Queue is Empty\n";
			return -1;
		}

		int key = head->next->key;
		NODE *temp = head;
		head = head->next;
		delete temp;

		deqL.unlock();
		return key;
	};

	virtual void Clear() {
		NODE *temp;
		while (tail != head)
		{
			temp = head;
			head = head->next;
			delete temp;
		}
		tail = head = new NODE;
	};

	virtual void Print20() {
		NODE *ptr = head->next;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 성긴 동기화\n\n", typeid(*this).name()); };
};

class LFNODE {
public:
	int key;
	LFNODE * next = nullptr;

	LFNODE() {}
	LFNODE(int k) : key{ k }, next{ nullptr } {}
	~LFNODE() {}
	
	bool CAS(int old_value, int new_value) {
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int *>(&next), &old_value, new_value);
	}
	
	bool CAS(LFNODE * currValue, LFNODE * wantResultValue) {
		int oldValue = reinterpret_cast<int>(currValue);
		int newValue = reinterpret_cast<int>(wantResultValue);
		return CAS(oldValue, newValue);
	}
};

bool CAS(LFNODE * volatile * wantToChange, LFNODE * currValue, LFNODE * wantResultValue) {
	int oldValue = reinterpret_cast<int>(currValue);
	int newValue = reinterpret_cast<int>(wantResultValue);
	return atomic_compare_exchange_strong(reinterpret_cast<atomic_int volatile *>(wantToChange), &oldValue, newValue);
}

class Nonblocking_Queue : public Virtual_Class
{
	LFNODE * volatile head;
	LFNODE * volatile tail;
public:
	Nonblocking_Queue() {
		tail = head = new LFNODE;
		tail->next = nullptr;
	}
	~Nonblocking_Queue() {
		Clear();
		if (nullptr != head) { delete head; }
	}
	
private:
	virtual void Enqueue(int key) {
		LFNODE *newNode = new LFNODE{ key };
		while (true)
		{
			LFNODE *last = tail;
			LFNODE *next = last->next;
			if (last != tail) { continue; }
			if (nullptr == next) {
				if (true == CAS(&(last->next), nullptr, newNode)) {
					CAS(&tail, last, newNode);
					return;
				}
			}
			else {
				// 만약 tail 위치가 last 위치랑 다르다고 하면, 고집 부릴 필요 없다. 과거로 덮어쓰기 때문.
				CAS(&tail, last, next);
			}
		}
	};

	virtual int Dequeue() {

		while (true)
		{
			LFNODE *first = head;
			LFNODE *last = tail;
			LFNODE *next = first->next;
			if (first != head) { continue; }
			if (first == last) {
				if (nullptr == next) { return -1; }
				CAS(&tail, last, next);
				continue;
			}
			int key = next->key;
			if (false == CAS(&head, first, next)) { continue; }
			delete first;
			return key;
		}
	};

	virtual void Clear() {
		LFNODE *temp;
		while (nullptr != head)
		{
			temp = head;
			head = temp->next;
			delete temp;
		}
		tail = head = new LFNODE;
	};

	virtual void Print20() {
		LFNODE *ptr = head;
		for (int i = 0; i < 20; ++i) {
			if (tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 비멈춤 동기화 ( ABA 문제가 발생한다 )\n\n", typeid(*this).name()); };
};

using PSNODE = class lockFreeStampNode {
public:
	NODE * volatile ptr;
	int stamp;

	lockFreeStampNode(NODE *node_ptr) {
		ptr = node_ptr;
		stamp = 0;
	}

	lockFreeStampNode(NODE *node_ptr, int new_stamp) {
		ptr = node_ptr;
		stamp = new_stamp;
	}

	lockFreeStampNode() { ptr = nullptr; stamp = 0; }
	~lockFreeStampNode() { /*if (nullptr != ptr) delete ptr;*/ }

	void operator=(NODE* other) {
		ptr = other;
	}
};

bool CAS(PSNODE *addr, PSNODE * old_v, NODE * next) {
	PSNODE new_psnode(next, old_v->stamp + 1);

	long long temp = InterlockedCompareExchange64(
		reinterpret_cast<volatile long long *>(addr),
		*reinterpret_cast<long long *>(&new_psnode),
		*reinterpret_cast<long long *>(old_v));

	return temp == *reinterpret_cast<long long *>(old_v);
}

class Lock_free_stamp_Queue : public Virtual_Class
{
	PSNODE head;
	PSNODE tail;
public:
	Lock_free_stamp_Queue() {
		NODE * new_node = new NODE;
		head = new_node;
		tail = new_node;
	}
	~Lock_free_stamp_Queue() {
		Clear();
		if (nullptr != head.ptr) { delete head.ptr; }
	}

private:
	virtual void Enqueue(int key) {
		NODE *newNode = new NODE{ key };
		while (true)
		{
			PSNODE last = tail;
			NODE * next = last.ptr->next;
			if (last.ptr != tail.ptr) { continue; }
			if (nullptr == next) {
				if (true == CAS(&last, &tail, newNode)) {
					CAS(&tail, &last, newNode);
					return;
				}
			}
			else {
				CAS(&tail, &last, next);
			}
		}
	};

	virtual int Dequeue() {

		while (true)
		{
			PSNODE first = head;
			PSNODE last = tail;
			NODE *next = first.ptr->next;
			if (first.ptr != head.ptr) { continue; }
			if (first.ptr == last.ptr) {
				if (nullptr == next) { return -1; }
				CAS(&tail, &last, next);
				continue;
			}
			int key = next->key;
			if (false == CAS(&head, &first, next)) { continue; }
			delete first.ptr;
			return key;
		}
	};

	virtual void Clear() {
		PSNODE *temp;
		while (nullptr != head.ptr)
		{
			temp = &head;
			head.ptr = temp->ptr->next;
			delete temp->ptr;
		}
		NODE * new_node = new NODE;
		head = PSNODE(new_node);
		tail = PSNODE(new_node);
	};

	virtual void Print20() {
		NODE *ptr = head.ptr;
		for (int i = 0; i < 20; ++i) {
			if (tail.ptr == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 비멈춤 스탬프 동기화\n\n", typeid(*this).name()); };
};


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	//List_Classes.emplace_back(new CorseGrain_QUEUE());
	//List_Classes.emplace_back(new Nonblocking_Queue());
	List_Classes.emplace_back(new Lock_free_stamp_Queue());

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