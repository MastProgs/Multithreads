#pragma once

#include <chrono>

using namespace std;
using namespace chrono;

class Time_Check {
public:
	Time_Check() {};
	~Time_Check() {};
	void check() { m_t = high_resolution_clock::now(); }
	void check_end() { m_te = high_resolution_clock::now(); }
	void check_and_show() { check_end(); show(); }
	void show() { cout << " Time : " << duration_cast<milliseconds>(m_te - m_t).count() << " ms\n"; }
private:
	high_resolution_clock::time_point m_t;
	high_resolution_clock::time_point m_te;
};