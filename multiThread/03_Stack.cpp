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
		for (int i = 1; i < NUM_TEST / num_thread; ++i) {
			if ((1 & rand()) || (i < KEY_RANGE)) { Push(i); }
			else { Pop(); }
		}
	}

	virtual void Push(int) = 0;
	virtual int Pop() = 0;

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

bool CAS(NODE * volatile * wantToChange, NODE * currValue, NODE * wantResultValue) {
	int oldValue = reinterpret_cast<int>(currValue);
	int newValue = reinterpret_cast<int>(wantResultValue);
	return atomic_compare_exchange_strong(reinterpret_cast<atomic_int volatile *>(wantToChange), &oldValue, newValue);
}

class CorseGrain_STACK : public Virtual_Class
{
	NODE *top;
	mutex stackL;
public:

	CorseGrain_STACK() {
		top = nullptr;
	};
	~CorseGrain_STACK() {
		Clear();
	};

private:
	virtual void Push(int key) {
		NODE *e = new NODE{ key };
		stackL.lock();
		if (nullptr != top) { e->next = top; }
		top = e;
		stackL.unlock();
	};

	virtual int Pop() {
		NODE *temp = nullptr;
		int key = 0;
		stackL.lock();
		if (nullptr == top) {
			stackL.unlock();
			return key;
		}
		key = top->key;
		temp = top;
		top = top->next;
		delete temp;
		stackL.unlock();
		return key;
	};

	virtual void Clear() {
		NODE *temp;
		while (nullptr != top)
		{
			temp = top;
			top = top->next;
			delete temp;
		}
		top = nullptr;
	};

	virtual void Print20() {
		NODE *ptr = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 성긴 동기화\n\n", typeid(*this).name()); };
};

// 스왑시 CAS 이렇게 해야됨
// NODE *e = new NODE { key };
// NODE *ptr = top;
// e -> next = ptr;
// while ( false == CAS ( &top, ptr, e ) );

/*

 do{


 } while (
 *** pop

 do{
	if(nullptr == top) { return0; }
	ptr = top;
	next = ptr->next;
	int res = ptr->key;
 } while ( false == CAS(&top, ptr, next));

 return res;
*/

class Lock_Free_STACK : public Virtual_Class
{
	NODE *top;
public:

	Lock_Free_STACK() {
		top = nullptr;
	};
	~Lock_Free_STACK() {
		Clear();
	};

private:
	virtual void Push(int key) {
		NODE *e = new NODE{ key };
		NODE *ptr;
		do {
			ptr = top;
			e->next = ptr;
		} while (false == CAS(&top, ptr, e));
	};

	virtual int Pop() {
		int key = 0;
		if (nullptr == top) { return key; }

		NODE *ptr = nullptr;
		NODE *next = nullptr;
		do
		{
			ptr = top;
			next = ptr->next;
			key = ptr->key;
		} while (false == CAS(&top, ptr, next));
		// delete ptr;	// ABA 문제 발생
		return key;
	};

	virtual void Clear() {
		NODE *temp;
		while (nullptr != top)
		{
			temp = top;
			top = top->next;
			delete temp;
		}
		top = nullptr;
	};

	virtual void Print20() {
		NODE *ptr = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 비멈춤 동기화 ( ABA 문제 발생으로 delete 안함 )\n\n", typeid(*this).name()); };
};

class BackOff {
private:
	int mindelay, maxdelay;
	int limit;
public:
	BackOff(int min, int max) {
		limit = mindelay = min;
		maxdelay = max;
	}
	~BackOff() {}

	void do_backoff() {
		int delay = rand() % limit;
		if (0 == delay) { return; }
		limit += limit;
		if (limit > maxdelay) limit = maxdelay;
		int now, curr;

		// VS 2015 부터 됨 ( 그 전엔 ns 단위를 제대로 지원 안했음 )
		auto start = high_resolution_clock::now();
		auto du = high_resolution_clock::now() - start;
		while (duration_cast<nanoseconds>(du).count() < delay)
		{
			auto du = high_resolution_clock::now() - start;
		}
	}
};

class Lock_Free_BackOff_STACK : public Virtual_Class
{
	NODE *top;
public:

	Lock_Free_BackOff_STACK() {
		top = nullptr;
	};
	~Lock_Free_BackOff_STACK() {
		Clear();
	};

private:
	virtual void Push(int key) {
		NODE *e = new NODE{ key };
		NODE *ptr;
		do {
			ptr = top;
			e->next = ptr;
		} while (false == CAS(&top, ptr, e));
	};

	virtual int Pop() {
		int key = 0;
		if (nullptr == top) { return key; }

		NODE *ptr = nullptr;
		NODE *next = nullptr;
		do
		{
			ptr = top;
			next = ptr->next;
			key = ptr->key;
		} while (false == CAS(&top, ptr, next));
		// delete ptr;	// ABA 문제 발생
		return key;
	};

	virtual void Clear() {
		NODE *temp;
		while (nullptr != top)
		{
			temp = top;
			top = top->next;
			delete temp;
		}
		top = nullptr;
	};

	virtual void Print20() {
		NODE *ptr = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	};

	virtual void myTypePrint() { printf(" %s == 비멈춤 BackOff 동기화 ( ABA 문제 발생으로 delete 안함 )\n\n", typeid(*this).name()); };
};

int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	//List_Classes.emplace_back(new CorseGrain_STACK());
	//List_Classes.emplace_back(new Lock_Free_STACK());
	List_Classes.emplace_back(new Lock_Free_BackOff_STACK());

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