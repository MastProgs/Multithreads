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

	virtual void myTypePrint() { printf(" %s == ���� ����ȭ\n\n", typeid(*this).name()); }

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

	// �ش� key ���� �ֽ��ϱ�?
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

	// ���� ��� ���� ����
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

	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
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

	virtual void myTypePrint() { printf(" %s == ������ ����ȭ\n\n", typeid(*this).name()); }

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

	// �ش� key ���� �ֽ��ϱ�?
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

	// ���� ��� ���� ����
	virtual void Clear() {
		NODE *ptr;
		while (head.next != &tail)
		{
			ptr = head.next;
			head.next = ptr->next;
			delete ptr;
		}
	}

	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
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
	// �˻��� ���� ������尡 �ʹ� ũ��, �ϴ� �˻��� ��. ����� �ߴ��� Ȯ����
	// �߸��Ǿ�����, ó������ �ٽ� -> ��õ�� ����ȭ�� ���� (�õ��� ��ġ�� ����� ���� �� ����)
	// ���1. �̵��߿� ���ڱ� ���� �����ع����� ��� �ɰ��ΰ�? delete �� ��带 ����Ų��? ����
	// ����� �� �����Ͷ�� ������ ���� ������ �� ó������� �Ѵ�.
	// �׷��� ������, ���� �ϴµ� DELETE �� ���� �ʰ�, �ٸ� ���� ���д�. ( �����ʹ� �״�δ� ������ )
	
	// ���� �����ؼ�, �ι�° ������ �ٽ� �� �� �ִ��� Ȯ�� �Ѵ�.
	// ù��° �˻��� �ƹ��� �� ���� �˻�������, ������ ���� ��ġ ���� �ɾ� ���� �ٽ� �˻���
	
	/********* ������ �޸� ������ �ִٴ� �� *********/

	NODE head, tail;
public:
	Optimistic_synchronization_LIST() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Optimistic_synchronization_LIST() { Clear(); }

	virtual void myTypePrint() { printf(" %s == ��õ�� ����ȭ, �޸𸮸� �������� �����Ƿ� ������ �ȴ�.\n\n", typeid(*this).name()); }

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

	// �ش� key ���� �ֽ��ϱ�?
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

	// ���� ��� ���� ����
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

	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
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
	// ��õ�� ����ȭ�� ���� -> marked �� ����Ͽ� ���� �������� ���� ����

	// free-list �� ������ �ʾƼ�, ���ó� ����

	NODE head, tail;
public:
	Lazy_synchronization_List() { head.key = 0x80000000; head.next = &tail; tail.key = 0x7fffffff; tail.next = nullptr; }
	~Lazy_synchronization_List() { Clear(); }

	virtual void myTypePrint() { printf(" %s == ������ ����ȭ, ���ó� free-list �� ������ �ʾƼ� ����\n\n", typeid(*this).name()); }

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

	// ���� ��� ���� ����
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

	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
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

	// �Ϲ� �����Ϳ��� ->next ������ atomic �ϰ� �ѹ��� 32bit ������ ���ư��� ������ �߰����� ����.
	// ������, shared_ptr ���� ���� ���� �׷��� �ʴ�. �߰����� �����Ѵ�. 2 stack ���� �а� ����. ( 8 bytes )
	// �߰��� �����Ͱ� �����ϱ� ������, �а� ���� �߰��� ������ �߻��� �� �ۿ� ����.

	// �ذ�å 1. lock �� �Ǵ� ! (������ ���� �˸���ũ)
	// �ذ�å 2. atomic_shared_ptr �� ����. ( ������ �̷��� ����. �ٸ� C++20 ǥ�� ���� ) -> �Ĵ� ���� �ִ�. ���ɵ� ������!
	// �ذ�å 3. atomic �� shared_ptr �� �����.
	// �ذ�å 4. atomic operation ( atomic stom, atomic load ���� �Լ��� ���� �ִ�. �۵��� atomic �ϰ�.. )
	shared_ptr<NODE> head, tail;
public:
	Shared_ptr_Lazy_synchronization_List() { head = make_shared<NODE>(0x80000000, 0); tail = make_shared<NODE>(0x7fffffff, 0); head->shared = tail; tail->shared = nullptr; }
	~Shared_ptr_Lazy_synchronization_List() { Clear(); head = nullptr; tail = nullptr; }

	virtual void myTypePrint() { printf(" %s == ������ ����ȭ, shared_ptr\n\n", typeid(*this).name()); }

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

	// ���� ��� ���� ����
	virtual void Clear() {
		head->shared = tail;
	}

	// ���� ����� ���� Ȯ���ϱ� ���� �⺻ ���� 20�� üũ
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
			// ������ ������ �ذ��Ϸ���, �� ��帶�� lock �� �ɾ�� �ϴµ�...
			// �׷��� �Ǹ� ��ǻ� ������ ����ȭ�� �ٸ��ٰ� ����.

			// C++ ���� atomic_store �� ���� �ڵ� ��ȯ ������ �������� �����Ƿ�, ���� Node �� �����ؾ���
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

// Coarse_grained_synchronization_LIST c_set; // ���� ����ȭ

// Fine_grained_synchronization_LIST f_set; // ������ ����ȭ

// Optimistic_synchronization_LIST // ��õ�� ����ȭ

// Lazy_synchronization_List // ������ ����ȭ

// Nonblocking_synchronization_List // ����� ����ȭ


int main() {
	setlocale(LC_ALL, "korean");
	vector<Virtual_Class *> List_Classes;

	//List_Classes.emplace_back(new Coarse_grained_synchronization_LIST());
	//List_Classes.emplace_back(new Fine_grained_synchronization_LIST());
	//List_Classes.emplace_back(new Optimistic_synchronization_LIST());	// �޸� ������ �ִ�.
	//List_Classes.emplace_back(new Lazy_synchronization_List());	// �޸� ������ �ִ�.
	List_Classes.emplace_back(new Shared_ptr_Lazy_synchronization_List());	// ���ٰ� ����

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