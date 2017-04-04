#include <iostream>
#include <typeinfo>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <atomic>

#define NUM_TEST 4000000
#define KEY_RANGE 1000;

using namespace std;
using namespace chrono;

class Time_Check {
public:
	Time_Check() {};
	~Time_Check() {};
	void check() { m_t = high_resolution_clock::now(); }
	void check_end() { m_te = high_resolution_clock::now(); }
	void check_and_show() { check_end(); show(); }
	void show(){ cout << " Time : " << duration_cast<milliseconds>(m_te - m_t).count() << " ms\n";	}
private:
	high_resolution_clock::time_point m_t;
	high_resolution_clock::time_point m_te;
};

class NODE {
public:
	int key;
	NODE *next;
	mutex m_L;

	NODE() { next = nullptr; };
	NODE(int x) : key(x) { next = nullptr; };
	~NODE() {};

	void Lock() { m_L.lock(); }
	void UnLock() { m_L.unlock(); }
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

class Coarse_grained_synchronization_LIST : public Virtual_Class {
	NODE head, tail;
	mutex m_lock;
	bool mutex_on{ true };
public:
	Coarse_grained_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Coarse_grained_synchronization_LIST() { Clear(); }

	virtual void myTypePrint() { printf(" %s == 성긴 동기화\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		if (x == curr->key) {
			if (true == mutex_on) { m_lock.unlock(); }
			return false;
		}
		else {
			NODE *node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			if (true == mutex_on) { m_lock.unlock(); }
			return true;
		}
	};

	virtual bool Remove(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;
		if (x != curr->key) {
			if (true == mutex_on) { m_lock.unlock(); }
			return false; }
		else {
			prev->next = curr->next;
			delete curr;
			if (true == mutex_on) { m_lock.unlock(); }
			return true;
		}
	};

	// 해당 key 값이 있습니까?
	virtual bool Contains(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		if (x != curr->key) { 
			if (true == mutex_on) { m_lock.unlock(); }
			return false;
		}
		else {
			if (true == mutex_on) { m_lock.unlock(); }
			return true;
		}
	};

	// 내부 노드 전부 해제
	virtual void Clear() {
		if (true == mutex_on) { m_lock.lock(); }
		NODE *ptr;
		while (head.next != &tail)
		{
			ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
		if (true == mutex_on) { m_lock.unlock(); }
	}

	// 값이 제대로 들어갔나 확인하기 위한 기본 앞쪽 20개 체크
	virtual void Print20() {
		if (true == mutex_on) { m_lock.lock(); }
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		if (true == mutex_on) { m_lock.unlock(); }
		cout << "\n\n";
	}
private:
	
	NODE* Search_key(int key) {
		NODE *prev, *curr;

		prev = &head;
		if (true == mutex_on) { m_lock.lock(); }
		curr = head.next;
		while (curr->key < key) {
			prev = curr;
			curr = curr->next;
		}

		return prev;
	}
};

class Fine_grained_synchronization_LIST : public Virtual_Class {
	NODE head, tail;
	mutex m_lock;
public:
	Fine_grained_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Fine_grained_synchronization_LIST() {
		// head.m_L.try_lock.lock();
		
		Clear();
	}

	virtual void myTypePrint() { printf(" %s == 세밀한 동기화\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		if (x == curr->key) {
			prev->m_L.unlock();
			curr->m_L.unlock();
			return false;
		}
		else {
			NODE *node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			prev->m_L.unlock();
			curr->m_L.unlock();
			return true;
		}
	};

	virtual bool Remove(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;
		if (x != curr->key) {
			prev->m_L.unlock();
			curr->m_L.unlock();
			return false;
		}
		else {
			curr->next->m_L.lock();
			prev->next = curr->next;
			curr->next->m_L.unlock();
			curr->m_L.unlock();
			delete curr;
			prev->m_L.unlock();
			return true;
		}
	};

	// 해당 key 값이 있습니까?
	virtual bool Contains(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		if (x != curr->key) {
			prev->m_L.unlock();
			curr->m_L.unlock();
			return false;
		}
		else {
			prev->m_L.unlock();
			curr->m_L.unlock();
			return true;
		}
	};

	// 내부 노드 전부 해제
	virtual void Clear() {
		NODE *ptr;
		while (head.next != &tail)
		{
			ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
	}

	// 값이 제대로 들어갔나 확인하기 위한 기본 앞쪽 20개 체크
	virtual void Print20() {
		NODE *ptr = head.next;
		for (int i = 0; i < 20; ++i) {
			if (&tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->next;
		}
		cout << "\n\n";
	}
private:
	NODE* Search_key(int key) {
		NODE *prev, *curr;

		head.m_L.lock();
		prev = &head;

		head.next->m_L.lock();
		curr = head.next;

		while (curr->key < key) {

			prev->m_L.unlock();
			prev = curr;

			curr->next->m_L.lock();
			curr = curr->next;
		}
		return prev;
	}
};



// Coarse_grained_synchronization_LIST c_set; // 성긴 동기화
/// no Lock
// 1 Thread 1454 ms
/// mutex Lock on
// 1 - 2065
// 2 - 2293
// 4 - 2776
// 8 - 2759
// 16 - 2786

// Fine_grained_synchronization_LIST f_set; // 세밀한 동기화

// Optimistic_synchronization_LIST // 낙천적 동기화

// Lazy_synchronization_List // 게으른 동기화

// Nonblocking_synchronization_List // 비멈춤 동기화


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	List_Classes.emplace_back(new Coarse_grained_synchronization_LIST());
	List_Classes.emplace_back(new Fine_grained_synchronization_LIST());

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
		}
		cout << "\n---- Next Class ----\n";
	}

	for (auto classes : List_Classes) { delete classes; }
}