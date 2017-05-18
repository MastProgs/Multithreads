#include <iostream>
#include <typeinfo>
#include <thread>
#include "TimeCheck.h"
#include <mutex>
#include <vector>
#include <atomic>

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
	NODE head;
	NODE *tail;

	mutex enqL;
	mutex deqL;
public:

	CorseGrain_QUEUE() {
		tail = &head;
		tail->next = nullptr;
	};
	~CorseGrain_QUEUE() { Clear(); };

private:
	virtual void Enqueue(int key) {
		enqL.lock();
		tail->next = new NODE{ key };
		tail = tail->next;
		enqL.unlock();
	};

	virtual int Dequeue() {
		deqL.lock();
		if (nullptr == head.next) {
			deqL.unlock();
			cout << "Queue is Empty\n";
			return -1;
		}

		NODE *temp = head.next;
		int key = temp->key;
		head.next = temp->next;
		delete temp;

		deqL.unlock();
		return key;
	};

	virtual void Clear() {
		NODE *temp;
		while (nullptr != head.next)
		{
			temp = head.next;
			head.next = temp->next;
			delete temp;
		}
		tail = &head;
	};

	virtual void Print20() {
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 성긴 동기화\n\n", typeid(*this).name()); };
};

int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	List_Classes.emplace_back(new CorseGrain_QUEUE());

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