#include <workspace/workspace.h>

// self-defined exception machenism
class excep: public std::exception {
    const char* err;
public:
     excep(const char* err): err(err) {}
    ~excep() = default; 

    const char* what() const noexcept override {
        return err;
    }
}; 

int main() 
{
    wsp::workbranch wbr;

    wbr.submit([]{ throw std::logic_error("A logic error"); });     // log error
    wbr.submit([]{ throw std::runtime_error("A runtime error"); }); // log error
    wbr.submit([]{ throw excep("XXXX");});       // log error

    auto future1 =  wbr.submit([]{ throw std::bad_alloc(); return 1; }); // catch error
    auto future2 =  wbr.submit([]{ throw excep("YYYY"); return 2; });    // catch error

    try {
        future1.get();
    } catch (std::exception& e) {
        std::cerr<<"Caught error: "<<e.what()<<std::endl;
    }
    try {
        future2.get();
    } catch (std::exception& e) {
        std::cerr<<"Caught error: "<<e.what()<<std::endl;
    }

}