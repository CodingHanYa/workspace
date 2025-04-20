#include <iostream>
#include <workspace/utility.hpp>
#include <cassert>
using namespace wsp::details;
using namespace std;

class SmallTask {
public:
    SmallTask() {
        cout<<"SmallTask: construct"<<endl;
    }
    SmallTask(const SmallTask&) {
        cout<<"SmallTask: copy construct"<<endl;
    }
    SmallTask(SmallTask&&) {
        cout<<"SmallTask: move construct"<<endl;
    }
    ~SmallTask() {
        cout<<"SmallTask: distruct"<<endl;
    }
    void operator()() {
        cout<<"SmallTask: run"<<endl;
    }
private:
    char a[64 - 2 * sizeof(void*)]; 
};

class BigTask {
public:
    BigTask() {
        cout<<"BigTask: construct"<<endl;
    }
    BigTask(const BigTask&) {
        cout<<"BigTask: copy construct"<<endl;
    }
    BigTask(BigTask&&) {
        cout<<"BigTask: move construct"<<endl;
    }
    ~BigTask() {
        cout<<"BigTask: distruct"<<endl;
    }
    void operator()() {
        cout<<"BigTask: run"<<endl;
    }
private:
    char a[64];
};

template <typename T>
void safe_exec(T& e) {
    try {
        e();
    } catch (std::exception& e) {
        cout<<"Got exception: "<<e.what()<<endl;
    }
}

void seperate() {
    cout<<"---------------------------------\n";
}

int main() {
    // Empty
    {
        seperate();
        task_t a;
        cout<<"sizeof(task_t a) = " << sizeof(a) << endl;
        assert(!a);
        task_t b = a;
        assert(!b);
        b.reset();
        task_t c = std::move(b);
        assert(!c);
    }
    // Use stack
    {
        seperate();
        cout<<"sizeof(SmallTask) = "<< sizeof(SmallTask) << endl;
        task_t a{SmallTask()};
        cout<<"sizeof(task_t a) = " << sizeof(a) << endl;
        task_t b = a;
        assert(a);
        a();
        task_t c = std::move(b);
        assert(!b);
        assert(c);
    }
    // Use heap
    {
        seperate();
        cout<<"sizeof(BigTask) = "<< sizeof(BigTask) << endl;
        task_t a{BigTask()};
        cout<<"sizeof(task_t a) = " << sizeof(a) << endl;
        task_t b = a;
        assert(a);
        a();
        task_t c = std::move(b);
        assert(!b);
        assert(c);
    } 
    // check
    {
        seperate();
        function_<void(int), 64> a([](int){});
        function_<void(int), 64> b([](int){});
        cout<<a.inline_size<<endl;
        cout<<b.inline_size<<endl;
        // a = b;
        b = a;
    }
}
