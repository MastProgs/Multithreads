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
	mutex m_L;
public:
	int key;
	NODE *next;
	shared_ptr<NODE> shared;
	bool marked = { false };

	NODE() { next = nullptr; };
	NODE(int x) : key(x) { next = nullptr; };
	NODE(int x, int) : key(x) { shared = nullptr; };
	~NODE() {};

	void Lock() { m_L.lock(); }
	void Unlock() { m_L.unlock(); }
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
			prev->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			NODE *node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			prev->Unlock();
			curr->Unlock();
			return true;
		}
	};

	virtual bool Remove(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;
		if (x != curr->key) {
			prev->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			curr->Unlock();
			delete curr;
			prev->Unlock();
			return true;
		}
	};

	// 해당 key 값이 있습니까?
	virtual bool Contains(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		if (x != curr->key) {
			prev->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			prev->Unlock();
			curr->Unlock();
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

		head.Lock();
		prev = &head;

		head.next->Lock();
		curr = head.next;

		while (curr->key < key) {

			prev->Unlock();
			prev = curr;

			curr->next->Lock();
			curr = curr->next;
		}
		return prev;
	}
};

class Optimistic_synchronization_LIST : public Virtual_Class {
	// 검색시 락은 오버헤드가 너무 크니, 일단 검색후 락. 제대로 했는지 확인후
	// 잘못되었으면, 처음부터 다시 -> 낙천적 동기화의 개념 (시도한 위치가 제대로 잡힐 때 까지)
	// 고민1. 이동중에 갑자기 누가 삭제해버리면 어떻게 될것인가? delete 된 노드를 가르킨다? ㅁㅊ
	// 제대로 된 포인터라는 보장이 없기 때문에 잘 처리해줘야 한다.
	// 그렇기 때문에, 빼긴 하는데 DELETE 를 하지 않고, 다른 곳에 빼둔다. ( 포인터는 그대로니 안정성 )
	
	// 만약 실패해서, 두번째 왔을때 다시 올 수 있는지 확인 한다.
	// 첫번째 검색은 아무런 락 없이 검색하지만, 다음은 현재 위치 락을 걸어 놓고 다시 검사함
	
	/********* 문제는 메모리 누수가 있다는 점 *********/

	NODE head, tail;
public:
	Optimistic_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Optimistic_synchronization_LIST() { Clear(); }

	virtual void myTypePrint() { printf(" %s == 낙천적 동기화, 메모리를 해제하지 않으므로 누수가 된다.\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		NODE *prev, *curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->next;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				NODE *node = new NODE{ x };
				node->next = curr;
				prev->next = node;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	virtual bool Remove(int x) {
		NODE *prev, *curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->next;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				prev->Unlock();
				curr->Unlock();
				continue;;
			}

			if (x != curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				prev->next = curr->next;
				//free_list.insert(curr);
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	// 해당 key 값이 있습니까?
	virtual bool Contains(int x) {
		NODE *prev, *curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->next;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x != curr->key) {
				prev->Unlock();
				curr->Unlock();
				return false;
			}
			else {
				prev->Unlock();
				curr->Unlock();
				return true;
			}
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

		//free_list.Clear();
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

		prev = &head;
		curr = head.next;

		while (curr->key < key) {
			prev = curr;
			curr = curr->next;
		}
		return prev;
	}

	bool validate(NODE *prev, NODE *curr) {	return (Search_key(curr->key)->next == curr);}
};

class Lazy_synchronization_List : public Virtual_Class {
	// 낙천적 동기화와 동일 -> marked 를 사용하여 삭제 진행중인 것을 구분

	// free-list 를 만들지 않아서, 역시나 누수

	NODE head, tail;
public:
	Lazy_synchronization_List() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Lazy_synchronization_List() { Clear(); }

	virtual void myTypePrint() { printf(" %s == 게으른 동기화, 역시나 free-list 를 만들지 않아서 누수\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		NODE *prev, *curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->next;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				NODE *node = new NODE{ x };
				node->next = curr;
				prev->next = node;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	virtual bool Remove(int x) {
		NODE *prev, *curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->next;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;;
			}

			if (x != curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				curr->marked = true;
				prev->next = curr->next;
				//free_list.insert(curr);
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	virtual bool Contains(int x) {
		NODE *prev, *curr;

		prev = Search_key(x);
		curr = prev->next;

		return ((curr->key == x) && (!curr->marked));
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

		//free_list.Clear();
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

		prev = &head;
		curr = head.next;

		while (curr->key < key) {
			prev = curr;
			curr = curr->next;
		}
		return prev;
	}

	bool validate(NODE *prev, NODE *curr) {
		return ((!prev->marked) && (!curr->marked) && (prev->next == curr));
	}
};

class Shared_ptr_Lazy_synchronization_List : public Virtual_Class {
	// shared_ptr

	// 일반 포인터에서 ->next 같은게 atomic 하게 한번에 32bit 값으로 돌아가기 때문에 중간값이 없다.
	// 하지만, shared_ptr 같은 경우는 절대 그렇지 않다. 중간값이 존재한다. 2 stack 으로 읽고 쓴다. ( 8 bytes )
	// 중간값 포인터가 존재하기 때문에, 읽고 쓰는 중간에 문제가 발생할 수 밖에 없다.

	// 해결책 1. lock 을 건다 ! (하지만 성능 똥망테크)
	// 해결책 2. atomic_shared_ptr 를 쓴다. ( 하지만 이런건 없다. 다만 C++20 표준 제안 ) -> 파는 곳이 있다. 성능도 괜찮다!
	// 해결책 3. atomic 한 shared_ptr 를 만든다.
	// 해결책 4. atomic operation ( atomic stom, atomic load 같은 함수가 따로 있다. 작동을 atomic 하게.. )
	shared_ptr<NODE> head, tail;
public:
	Shared_ptr_Lazy_synchronization_List() { head = make_shared<NODE>(0x80000000, 0); tail = make_shared<NODE>(0x7fffffff, 0); head->shared = tail; tail->shared = nullptr; }
	~Shared_ptr_Lazy_synchronization_List() { Clear(); head = nullptr; tail = nullptr; }

	virtual void myTypePrint() { printf(" %s == 게으른 동기화, shared_ptr\n\n", typeid(*this).name()); }

	virtual bool Add(int x) {
		shared_ptr<NODE> prev, curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->shared;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x == curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				shared_ptr<NODE> node = make_shared<NODE>(x, 0);
				node->shared = curr;
				prev->shared = node;
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	virtual bool Remove(int x) {
		shared_ptr<NODE> prev, curr;

		while (true)
		{
			prev = Search_key(x);
			curr = prev->shared;

			prev->Lock();
			curr->Lock();

			if (false == validate(prev, curr)) {
				curr->Unlock();
				prev->Unlock();
				continue;
			}

			if (x != curr->key) {
				curr->Unlock();
				prev->Unlock();
				return false;
			}
			else {
				curr->marked = true;
				prev->shared = curr->shared;
				//free_list.insert(curr);
				curr->Unlock();
				prev->Unlock();
				return true;
			}
		}
	};

	virtual bool Contains(int x) {
		shared_ptr<NODE> prev, curr;

		prev = Search_key(x);
		curr = prev->shared;

		return ((curr->key == x) && (!curr->marked));
	};

	// 내부 노드 전부 해제
	virtual void Clear() {
		head->shared = tail;
	}

	// 값이 제대로 들어갔나 확인하기 위한 기본 앞쪽 20개 체크
	virtual void Print20() {
		shared_ptr<NODE> ptr = head->shared;
		for (int i = 0; i < 20; ++i) {
			if (tail == ptr) { break; }
			cout << ptr->key << " ";
			ptr = ptr->shared;
		}
		cout << "\n\n";
	}
private:
	shared_ptr<NODE> Search_key(int key) {
		shared_ptr<NODE> prev, curr;

		prev = head;
		curr = head->shared;

		while (curr->key < key) {
			// 터지는 문제를 해결하려면, 각 노드마다 lock 을 걸어야 하는데...
			// 그렇게 되면 사실상 세밀한 동기화랑 다를바가 없다.

			// C++ 에서 atomic_store 와 같은 자동 변환 연산을 제공하지 않으므로, 따로 Node 를 수정해야함
			/// *(reinterpret_cast<long long *>(prev)) atomic_store( reinterpret_cast<long long>(curr) );
			prev = curr;
			curr = curr->shared;
		}
		return prev;
	}

	bool validate(const shared_ptr<NODE> &prev, const shared_ptr<NODE> &curr) {
		return ((!prev->marked) && (!curr->marked) && (prev->shared == curr));
	}
};

// Coarse_grained_synchronization_LIST c_set; // 성긴 동기화

// Fine_grained_synchronization_LIST f_set; // 세밀한 동기화

// Optimistic_synchronization_LIST // 낙천적 동기화

// Lazy_synchronization_List // 게으른 동기화

// Nonblocking_synchronization_List // 비멈춤 동기화


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	//List_Classes.emplace_back(new Coarse_grained_synchronization_LIST());
	//List_Classes.emplace_back(new Fine_grained_synchronization_LIST());
	//List_Classes.emplace_back(new Optimistic_synchronization_LIST());	// 메모리 누수가 있다.
	//List_Classes.emplace_back(new Lazy_synchronization_List());	// 메모리 누수가 있다.
	List_Classes.emplace_back(new Shared_ptr_Lazy_synchronization_List());	// 돌다가 터짐

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