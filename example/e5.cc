#include <workspace/workspace.h>
#define TASKS 1000
#define SLEEP_FOR(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))

int main() {
    wsp::workbranch main_br("Main  ");
    wsp::workbranch help_br("Helper");

    wsp::supervisor main_br_sp(2, 4);  // interval 500(ms)
    wsp::supervisor help_br_sp(0, 2);  // interval 500(ms)
    main_br_sp.enable_log();  
    help_br_sp.enable_log();

    main_br_sp.supervise(main_br);
    help_br_sp.supervise(help_br);

    for (int i = 0; i < TASKS; ++i) {
        if (main_br.num_tasks() > 200) {
            if (help_br.num_tasks() > 200)
                SLEEP_FOR(20);
            else 
                help_br.submit([]{SLEEP_FOR(10);});
        } else {
            main_br.submit([]{SLEEP_FOR(10);});
        }
    }
    main_br.wait_tasks();
    help_br.wait_tasks();
}