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

		NODE *ptr = nullptr;
		NODE *next = nullptr;
		do
		{
			if (nullptr == top) { return key; }
			ptr = top;
			next = ptr->next;
			key = ptr->key;
		} while (false == CAS(&top, ptr, next));
		delete ptr;	// ABA 문제 발생
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
		/*auto start = high_resolution_clock::now();
		auto du = high_resolution_clock::now() - start;
		while (duration_cast<nanoseconds>(du).count() < delay)
		{
			du = high_resolution_clock::now() - start;
		}*/

		// resolution 오버헤드 좀 커서 그냥 간단한 아래 문법으로 대체

		_asm mov ecx, delay;
	myloop:
		_asm loop myloop;
	}
};

// 아직 backoff 구현이 전혀 안되었음 ( Back Off = 매번 CAS 를 시도하여 충돌하는 횟수를 줄이고자 랜덤한 시간차를 주어 충돌을 피하는 기법 )
static const auto MIN_DELAY = 200;
static const auto MAX_DELAY = 200000;

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
		BackOff backoff(MIN_DELAY, MAX_DELAY);

		NODE *e = new NODE{ key };
		NODE *ptr;
		do {
			ptr = top;
			e->next = ptr;

			if (true == CAS(&top, ptr, e)) { break; }
			backoff.do_backoff();

		} while (true);
	};

	virtual int Pop() {
		int key = 0;

		NODE *ptr = nullptr;
		NODE *next = nullptr;
		BackOff backoff(MIN_DELAY, MAX_DELAY);
		do
		{
			if (nullptr == top) { return key; }
			ptr = top;
			next = ptr->next;
			key = ptr->key;

			if (true == CAS(&top, ptr, next)) { break; }
			backoff.do_backoff();

		} while (true);
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

// 소거 락프리 스택 ( Back off 성능 향상이 그닥이라, 만약 충돌나면 스레드 끼리 직접 데이터 교환 기법 )
// 소거 (exchange class)는 ABA 문제가 발생할 여지가 없다. ( 왜냐하면, 상태 검사만 하고 내용 알고리즘을 바꾸지 않기 때문 )
// 원본 Stack 구조에서는 계속 ABA 문제는 동일하게 있다.
// 어차피 또 exchange 안에서 CAS 로 충돌이 나는데, Back Off 보다 더 성능 향상이 있을까?? 

static const int MAX_CAP = 4;
static const int S_EMPTY = 0;
static const int S_WAIT = 1;
static const int S_BUSY = 2;

using statevalue = struct StateValue
{
	volatile int state;
	int value;
};

bool CAS(statevalue *addr, statevalue old_v, statevalue new_v) {
	long long temp = InterlockedCompareExchange64(
		reinterpret_cast<volatile long long *>(addr),
		*reinterpret_cast<long long *>(&new_v),
		*reinterpret_cast<long long *>(&new_v));
	return temp == *reinterpret_cast<long long *>(&old_v);
}

bool CAS(int * addr, int old, int next) {
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr), &old, next);
}

class Exchanger
{
	// 2개의 스레드가 여기서 값을 교환하며 주고 받아야 한다.

public:
	int exchange(int value, bool *time_out) {

		*time_out = false;
		auto start_t = high_resolution_clock::now();

		while (true){
			statevalue old_sv = sv;

			switch (old_sv.state) {
			case S_EMPTY: {
				statevalue new_sv{ S_WAIT, value };
				if (true == CAS(&sv, old_sv, new_sv)) {
					// 성공을 했다면, 상태가 S_WAIT 으로 바뀌고, 다른 스레드가 오길 기다려야 함 - blocking
					while (S_WAIT == sv.state) {
						// 한도 끝도 없이 기다릴 순 없다.
						auto du = high_resolution_clock::now() - start_t;
						if (du > 200ns) {
							*time_out = true;
							return 0;
						}
					}
					int res = sv.value;
					sv.state = S_EMPTY;
					return res;
				}
				else {
					// 상태 바꾸는 CAS 에 실패 했다면, 처음 부터 다시 해야 함
					continue;
				}
				break;
			}
			case S_WAIT: {
				// 누군가 와서 기다리고 있는 상황
				statevalue new_sv{ S_BUSY, value };
				if (true == CAS(&sv, old_sv, new_sv)) {
					// 성공했다면, 값을 잘 바꾸었다.
					return old_sv.value;
				}
				else {
					// 새치기 당해서 다시 루프 돌아야 됨
					continue;
				}

				break;
			}
			case S_BUSY: {
				// 신나게 이미 바꾸고 있는 중이므로...
				continue;
				break;
			}
			default: cout << "Exchanger INVALID STATE!!\n\n"; break;
			}
		}
		return value;
	}

	Exchanger() { sv.state = S_EMPTY; };
	~Exchanger() {};

private:
	// 공용 자료라 atomic 해야 한다. - 여기서는 CAS 64비트를 쓴다. ( 일반적이라면 맨 앞 2bit 와 정수 30bit 활용하는데 귀찮.. )
	statevalue sv;
};

class EliminationArray
{
public:
	EliminationArray() {}
	~EliminationArray() {}

	int visit(int value, int range, bool *timeout) {
		int index = rand() % range;
		return exchanger[index].exchange(value, timeout);
	}

private:
	Exchanger exchanger[MAX_CAP];
};

class Lock_Free_eliminate_STACK : public Virtual_Class
{
	NODE *volatile top;
	EliminationArray e_array;
	int range;
public:

	Lock_Free_eliminate_STACK() {
		range = 1;
		top = nullptr;
	};
	~Lock_Free_eliminate_STACK() {
		Clear();
	};

private:
	virtual void Push(int key) {
		NODE *e = new NODE{ key };
		NODE *ptr;
		do {
			ptr = top;
			e->next = ptr;
			if (true == CAS(&top, ptr, e)) { break; }

			bool is_timeout;
			int res = e_array.visit(key, range, &is_timeout);
			if (true == is_timeout) {
				// range 감소 시도 - range 를 여러 쓰레드에서 바꾸기 때문에 atomic 해야 한다.
				if (0 == (rand() % MAX_CAP)) {
					// 협업 해서 늘리고 줄이면 오버헤드이므로, 그냥 귀찮아서 랜덤
					int old_range = range;
					if (1 < old_range) {
						// 실패해도 알바가 아님. 어차피 딴 놈이 조정해주기 때문, 나도 같이 조정하면 과다 조정
						CAS(&range, old_range, old_range - 1);
					}
				}
				continue;
			}
			else {
				// range 증가 시도
				if (0 == (rand() % MAX_CAP)) {
					int old_range = range;
					if (MAX_CAP - 1 > range) { CAS(&range, old_range, old_range + 1); }
					if (res == 0) {
						// 결과가 0 이면 교환이 성공적, top 에게 넘겨준것
						break;
					}
				}
				continue;
			}

		} while (true);
	};

	virtual int Pop() {
		int key = 0;

		NODE *ptr = nullptr;
		NODE *next = nullptr;
		do
		{
			if (nullptr == top) { return key; }
			ptr = top;
			next = ptr->next;
			key = ptr->key;

			if (true == CAS(&top, ptr, next)) { break; }

			bool is_timeout;
			int res = e_array.visit(key, range, &is_timeout);
			if (true == is_timeout) {
				if (0 == (rand() % MAX_CAP)) {
					int old_range = range;
					if (1 < old_range) { CAS(&range, old_range, old_range - 1); }
				}
				continue;
			}
			else {
				if (0 == (rand() % MAX_CAP)) {
					int old_range = range;
					if (MAX_CAP - 1 > range) { CAS(&range, old_range, old_range + 1); }
					if (res != 0) {
						// 결과가 0 이면 교환이 실패, 상대방도 pop
						// 결과가 0이 아니라면, 정상적으로 교환한거임
						return res;
					}
				}
				continue;
			}

		} while (true);
		//delete ptr;	// ABA 문제 발생
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

	virtual void myTypePrint() { printf(" %s == 비멈춤 소거 동기화 ( ABA 문제 발생으로 delete 안함 )\n\n", typeid(*this).name()); };
};


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	//List_Classes.emplace_back(new CorseGrain_STACK());
	//List_Classes.emplace_back(new Lock_Free_STACK());
	List_Classes.emplace_back(new Lock_Free_BackOff_STACK());
	List_Classes.emplace_back(new Lock_Free_eliminate_STACK());

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