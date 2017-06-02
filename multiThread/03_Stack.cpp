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

	virtual void myTypePrint() { printf(" %s == ���� ����ȭ\n\n", typeid(*this).name()); };
};

// ���ҽ� CAS �̷��� �ؾߵ�
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
		delete ptr;	// ABA ���� �߻�
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

	virtual void myTypePrint() { printf(" %s == ����� ����ȭ ( ABA ���� �߻����� delete ���� )\n\n", typeid(*this).name()); };
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

		// VS 2015 ���� �� ( �� ���� ns ������ ����� ���� ������ )
		/*auto start = high_resolution_clock::now();
		auto du = high_resolution_clock::now() - start;
		while (duration_cast<nanoseconds>(du).count() < delay)
		{
			du = high_resolution_clock::now() - start;
		}*/

		// resolution ������� �� Ŀ�� �׳� ������ �Ʒ� �������� ��ü

		_asm mov ecx, delay;
	myloop:
		_asm loop myloop;
	}
};

// ���� backoff ������ ���� �ȵǾ��� ( Back Off = �Ź� CAS �� �õ��Ͽ� �浹�ϴ� Ƚ���� ���̰��� ������ �ð����� �־� �浹�� ���ϴ� ��� )
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
		// delete ptr;	// ABA ���� �߻�
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

	virtual void myTypePrint() { printf(" %s == ����� BackOff ����ȭ ( ABA ���� �߻����� delete ���� )\n\n", typeid(*this).name()); };
};

// �Ұ� ������ ���� ( Back off ���� ����� �״��̶�, ���� �浹���� ������ ���� ���� ������ ��ȯ ��� )
// �Ұ� (exchange class)�� ABA ������ �߻��� ������ ����. ( �ֳ��ϸ�, ���� �˻縸 �ϰ� ���� �˰����� �ٲ��� �ʱ� ���� )
// ���� Stack ���������� ��� ABA ������ �����ϰ� �ִ�.
// ������ �� exchange �ȿ��� CAS �� �浹�� ���µ�, Back Off ���� �� ���� ����� ������?? 

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
	// 2���� �����尡 ���⼭ ���� ��ȯ�ϸ� �ְ� �޾ƾ� �Ѵ�.

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
					// ������ �ߴٸ�, ���°� S_WAIT ���� �ٲ��, �ٸ� �����尡 ���� ��ٷ��� �� - blocking
					while (S_WAIT == sv.state) {
						// �ѵ� ���� ���� ��ٸ� �� ����.
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
					// ���� �ٲٴ� CAS �� ���� �ߴٸ�, ó�� ���� �ٽ� �ؾ� ��
					continue;
				}
				break;
			}
			case S_WAIT: {
				// ������ �ͼ� ��ٸ��� �ִ� ��Ȳ
				statevalue new_sv{ S_BUSY, value };
				if (true == CAS(&sv, old_sv, new_sv)) {
					// �����ߴٸ�, ���� �� �ٲپ���.
					return old_sv.value;
				}
				else {
					// ��ġ�� ���ؼ� �ٽ� ���� ���ƾ� ��
					continue;
				}

				break;
			}
			case S_BUSY: {
				// �ų��� �̹� �ٲٰ� �ִ� ���̹Ƿ�...
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
	// ���� �ڷ�� atomic �ؾ� �Ѵ�. - ���⼭�� CAS 64��Ʈ�� ����. ( �Ϲ����̶�� �� �� 2bit �� ���� 30bit Ȱ���ϴµ� ����.. )
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
				// range ���� �õ� - range �� ���� �����忡�� �ٲٱ� ������ atomic �ؾ� �Ѵ�.
				if (0 == (rand() % MAX_CAP)) {
					// ���� �ؼ� �ø��� ���̸� ��������̹Ƿ�, �׳� �����Ƽ� ����
					int old_range = range;
					if (1 < old_range) {
						// �����ص� �˹ٰ� �ƴ�. ������ �� ���� �������ֱ� ����, ���� ���� �����ϸ� ���� ����
						CAS(&range, old_range, old_range - 1);
					}
				}
				continue;
			}
			else {
				// range ���� �õ�
				if (0 == (rand() % MAX_CAP)) {
					int old_range = range;
					if (MAX_CAP - 1 > range) { CAS(&range, old_range, old_range + 1); }
					if (res == 0) {
						// ����� 0 �̸� ��ȯ�� ������, top ���� �Ѱ��ذ�
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
						// ����� 0 �̸� ��ȯ�� ����, ���浵 pop
						// ����� 0�� �ƴ϶��, ���������� ��ȯ�Ѱ���
						return res;
					}
				}
				continue;
			}

		} while (true);
		//delete ptr;	// ABA ���� �߻�
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

	virtual void myTypePrint() { printf(" %s == ����� �Ұ� ����ȭ ( ABA ���� �߻����� delete ���� )\n\n", typeid(*this).name()); };
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