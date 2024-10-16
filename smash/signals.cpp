#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <sys/wait.h>

using namespace std;

void ctrlCHandler(int sig_num) {
  cout << "smash: got ctrl-C" << endl;
  JobsList::JobEntry* job_entry = JobsList::getInstance().getJobInForeground();
  pid_t pid = -1;
  if (job_entry != nullptr)
  {
    pid = job_entry->getPid();
  }
  if(pid != -1)
  {
    if (kill(pid,SIGKILL) == -1)
    {
      perror("smash error: kill failed");
      return;
    }
    cout << "smash: process "<< pid <<" was killed" << endl;
  }
}
