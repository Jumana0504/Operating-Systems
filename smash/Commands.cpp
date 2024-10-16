#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <chrono>
#include <sys/syscall.h>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////COMMAND//////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


Command::Command(const char *cmd_line)
{
  this->cmd_line = new char[strlen(cmd_line) + 1];
  strcpy(this->cmd_line, cmd_line);
  this->args_num = _parseCommandLine(cmd_line,this->args);
  this->isBackground = _isBackgroundComamnd(cmd_line);
}

Command::~Command()
{
  for(int i = 0; i < COMMAND_MAX_ARGS + 1; ++i)
  {
    free(this->args[i]);
  }
}

char* Command::getCmdLine()
{
  return this->cmd_line;
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////BUILT_IN_COMMAND/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


BuiltInCommand::BuiltInCommand(const char *cmd_line,bool remove_bachground_sing) : Command(cmd_line)
{
  if (remove_bachground_sing && this->isBackground)
  {
    _removeBackgroundSign(this->cmd_line);
  }
  this->args_num = _parseCommandLine(this->cmd_line,this->args);
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////EXTERNAL_COMMAND/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


ExternalCommand::ExternalCommand(const char *cmd_line,std::string original_cmd_line , bool isAlias)
                                 : Command(cmd_line),original_cmd_line(original_cmd_line),isAlias(isAlias) {}

bool isComplex(std::string string)
{
  for(auto & ch : string)
  {
    if (ch  == '*' || ch == '?')
    {
      return true;
    }
  }
  return false;
}

void ExternalCommand::execute()
{
  std::string alias_cmd_line_orignal = this->cmd_line;
  if (isAlias)
  {
    alias_cmd_line_orignal = this->original_cmd_line;
  }
  pid_t pid = fork();
  if (pid < 0)
  {
    perror("smash error: fork failed");
    return;
  }
  else if (pid == 0)
  {
    std::string copy = this->cmd_line;
    _removeBackgroundSign((char*)copy.c_str());
    copy = _trim(copy);
    this->cmd_line = new char[string(copy).length()+1];
    strcpy(this->cmd_line,copy.c_str());
    this->args_num = _parseCommandLine(this->cmd_line,this->args);
    setpgrp();
    if (isComplex(this->cmd_line))
    {
      const char *argp[] = {"/bin/bash","-c",this->cmd_line,nullptr};
      if (execv(argp[0],(char * *)argp) == -1)
      {
        perror("smash error: execv failed");
        exit(1);
      }
    }
    else
    {
      if (execvp(this->args[0],this->args) == -1)
      {
        perror("smash error: execvp failed");
        exit(1);
      }
    }
  }
  else if (pid > 0)
  {
    if (this->isBackground)
    {
      JobsList::getInstance().addJob(alias_cmd_line_orignal,pid);
    }
    else
    {
      JobsList::getInstance().addJob(alias_cmd_line_orignal,pid,false);
      if (waitpid(pid, nullptr,0) == -1)
      {
        perror("smash error: waitpid failed");
        return;
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////REDIRECTION_COMMAND//////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute()
{
  std::regex pattern(R"(\S+\s*>>\s*\S+)");
  bool append = false;
  std::string str = this->cmd_line;
  size_t start = str.find_first_of(">");
  string cmd_line_1 = str.substr(0,start);
  size_t end = start;
  if (std::regex_search(str,pattern))
  {
    append = true;
    end++;
  }
  string path = _trim(str.substr(end+1));
  if (this->isBackground)
  {
    path = _trim(path.substr(0,path.length()-1));
  }
  Command * cmd = SmallShell::getInstance().CreateCommand(cmd_line_1.c_str());
  pid_t pid = fork();
  if (pid == -1)
  {
    perror("smash error: fork failed");
    return;
  }
  else if (pid == 0)
  {
    setpgrp();
    int fd;
    if (!append) // > symbol
    {
      fd  = open(path.c_str(),O_WRONLY | O_CREAT | O_TRUNC,0666);
    }
    else // >> symbol
    {
      fd  = open(path.c_str(),O_WRONLY | O_CREAT | O_APPEND,0666);
    }
    if (fd == -1)
    {
      perror("smash error: open failed");
      exit(1);
    }
    if (dup2(fd,1) == -1)
    {
      perror("smash error: dup failed");
      exit(1);
    }
    if(close(fd) == -1)
    {
      perror("smash error: close failed");
      exit(1);
    }
    cmd->execute();
    exit(0);
  }
  else if (pid > 0)
  {
    if(waitpid(pid,nullptr,0) == -1)
    {
      perror("smash error: waitpid failed");
      return;
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////LIST_DIR_COMMAND/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

ListDirCommand::ListDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ListDirCommand::execute()
{
  if (args_num > 2)
  {
    cerr << "smash error: listdir: too many arguments" << endl;
    return;
  }
  else 
  {
    char buffer[COMMAND_MAX_LENGTH];
    if(getcwd(buffer,COMMAND_MAX_LENGTH) == nullptr)
    {      
      perror("smash error: getcwd failed");
      return;
    }
    string  path = buffer;
    if (args_num == 2)
    {
      path = args[1];
    }
    std::vector<std::string> files;
    std::vector<std::string> directories;
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1)
    {
      perror("smash error: open failed");
      return;
    }
    struct linux_dirent {
      unsigned long  d_ino;
      unsigned long  d_off;
      unsigned short d_reclen;
      char           d_name[];
    };
    int bytes_num;
    struct linux_dirent *entry;
    while (true)
    {
      bytes_num = syscall(SYS_getdents, fd, buffer, sizeof(buffer));
      if (bytes_num == -1 )
      {
        perror("smash error: getdents failed");
        return;
      }
      else if (bytes_num == 0)
      {
        break;
      }
      int i = 0;
      while ( i < bytes_num )
      {
        entry = (struct linux_dirent *)(buffer + i);
        if (entry->d_name[0] == '\0')
        {
          break;
        }
        string entry_path = path + "/" + string(entry->d_name);
        struct stat status;
        if (lstat(entry_path.c_str(),&status) == -1)
        {
          perror("smash error: stat failed");
          return;
        }
        if (S_ISDIR(status.st_mode))
        {
          directories.push_back(string(entry->d_name));
        }
        if (S_ISREG(status.st_mode))
        {
          files.push_back(string(entry->d_name));
        }
        i += entry->d_reclen;
      }
    }
    std::sort(files.begin(), files.end());
    std::sort(directories.begin(), directories.end());
    for (string str : files)
    {
      cout << "file: " << str << endl;
    }
    for (string str : directories)
    {
      if (str.compare(".") == 0 || str.compare("..") == 0)
      {
        continue;
      }
      cout << "directory: " << str << endl;
    }
    if (close(fd) == -1)
    {
      perror("smash error: close failed");
      return;
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////GET_USER_COMMAND/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


GetUserCommand::GetUserCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetUserCommand::execute()
{
  if (args_num > 2)
  {
    cerr << "smash error: getuser: too many arguments" << endl;
    return;
  }
  else
  {
    if (!JobsList::getInstance().isJobExistsByPid(atoi(args[1])) &&
        SmallShell::getInstance().getSmashPid() != atoi(args[1]))
    {
      cerr << "smash error: getuser: process " << args[1] << " does not exist" << endl;
      return;
    }
    string path = "/proc/" + string(args[1]) + "/status";
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
      perror("smash error: open failed");
      return;
    }
    uid_t uid;
    gid_t gid;
    char buffer[COMMAND_MAX_LENGTH];
    int bytes_num;
    string file;
    bytes_num = read(fd,buffer,sizeof(buffer)-1);
    if (bytes_num == -1)
    {
      perror("smash error: read failed");
      return;
    }
    buffer[bytes_num] = '\0';
    while (bytes_num > 0)
    {
      file += buffer;
      bytes_num = read(fd,buffer,sizeof(buffer)-1);
      if (bytes_num == -1)
      {
        perror("smash error: read failed");
        return;
      }
      buffer[bytes_num] = '\0';
    }
    string line;
    while(true)
    {
      size_t end_line = file.find("\n");
      line = file.substr(0,end_line+1);
      if (line.substr(0, 5) == "Uid:\t")
      {
        uid = atoi(line.substr(5).c_str());
      }
      if (line.substr(0, 5) == "Gid:\t")
      {
        gid = atoi(line.substr(5).c_str());
      }
      file = file.substr(end_line+1);
      if (end_line == std::string::npos)
      {
        break;
      }
    }
    struct passwd *user = getpwuid(uid);
    struct group *group = getgrgid(gid);
    cout << "User: " << user->pw_name  << endl;
    cout << "Group: " <<  group->gr_name << endl;
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////WATCH_COMMAND////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


WatchCommand::WatchCommand(const char *cmd_line) : Command(cmd_line) {}

bool isNumber(std::string string)
{
  std::regex pattern(R"(-?\d+)");
  return std::regex_match(string,pattern);
}

void WatchCommand::execute()
{
  if(args_num < 2)
  {
    cerr << "smash error: watch: command not specified" << endl;
    return;
  }
  else if (atoi(args[1]) <= 0 && isNumber(args[1]))
  {
    cerr << "smash error: watch: invalid interval" << endl;
    return;
  }
  else if (args_num < 3 && isNumber(args[1]))
  {
    cerr << "smash error: watch: command not specified" << endl;
    return;
  }
  else
  {
    string cmd_line_2 = "sleep ";
    string cmd_line_1;
    int i = 1;
    if (isNumber(args[1]))
    {
      cmd_line_2 += args[1];
      i = 2;
    }
    else
    {
      cmd_line_2 += "2";
    }
    for(;i<=args_num;i++)
    {
      if(args[i] == NULL) break;
      cmd_line_1 += args[i];
      cmd_line_1 += " ";
    }
    if (this->isBackground)
    {
      _removeBackgroundSign((char*)cmd_line_1.c_str());
    } 
    Command* command = SmallShell::getInstance().CreateCommand(cmd_line_1.c_str());
    Command* sleep_command = SmallShell::getInstance().CreateCommand(cmd_line_2.c_str());
    Command* clear_command = SmallShell::getInstance().CreateCommand("clear");
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("smash error: fork failed");
      return;
    }
    else if (pid == 0)
    {
      setpgrp();
      if (this->isBackground)
      {
        int fd_devNull = open("/dev/null", O_WRONLY);  
        if (fd_devNull == -1)
        {
          perror("smash error: open failed");
          exit(1);
        }
        if(dup2(fd_devNull, 1) == -1 || dup2(fd_devNull,2) == -1)
        {
          perror("smash error: dup2 failed");
          exit(1);
        }
        if(close(fd_devNull) == -1)
        {
          perror("smash error: close failed");
          exit(1);
        }    
      }
      else
      {
        clear_command->execute();
      }
      while (true)
      {
        command->execute();
        sleep_command->execute();
      }
      exit(0);
    }
    else if (pid > 0)
    {
      if (this->isBackground)
      {
        JobsList::getInstance().addJob(this->cmd_line,pid);
      }
      else
      {
        JobsList::getInstance().addJob(this->cmd_line,pid,false);
        if (waitpid(pid, nullptr,0) == -1)
        {
          perror("smash error: waitpid failed");
          return;
        }
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////PIPE_COMMAND/////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute()
{
  std::regex pattern(R"(\|\s*&|\|&)");
  bool regular_pipe = true;
  string tmp = this->cmd_line;
  size_t start = tmp.find_first_of("|");
  string cmd_line_1 = tmp.substr(0,start);
  if (std::regex_search(tmp,pattern))
  {
    regular_pipe = false;
    start++;
  }
  string cmd_line_2 = tmp.substr(start+1);
  Command * command_1 = SmallShell::getInstance().CreateCommand(cmd_line_1.c_str());
  Command * command_2 = SmallShell::getInstance().CreateCommand(cmd_line_2.c_str());
  int my_pipe[2];
  pipe(my_pipe);
  pid_t pid_1 = fork();
  if (pid_1 < 0)
  {
    perror("smash error: fork failed");
    return;
  }
  else if (pid_1 == 0)
  {
    setpgrp();
    int fd_stdout_or_stderr = 1; 
    if (!regular_pipe)
    {
      fd_stdout_or_stderr = 2;
    }
    if (dup2(my_pipe[1],fd_stdout_or_stderr) == -1)
    {
      perror("smash error: dup failed");
      exit(1);
    }
    if (close(my_pipe[0]) == -1 || close(my_pipe[1]) == -1)
    {
      perror("smash error: close failed");
      exit(1);
    }
    command_1->execute();
    exit(0);
  }
  else if (pid_1 > 0)
  {
    if (!regular_pipe)
    {
      if (waitpid(pid_1,nullptr,0) == -1)
      {
        perror("smash error: waitpid failed");
        return;      
      }
    }
    pid_t pid_2 = fork();
    if (pid_2 < 0)
    {
      perror("smash error: fork failed");
      return;
    }
    else if (pid_2 == 0)
    {
      setpgrp();
      if (dup2(my_pipe[0],0) == -1)
      {
        perror("smash error: dup failed");
        exit(1);
      }
      if (close(my_pipe[0]) == -1 || close(my_pipe[1]) == -1)
      {
        perror("smash error: close failed");
        exit(1);
      }
      command_2->execute();
      exit(0);
    }
    else if (pid_2 > 0)
    {
      if (close(my_pipe[0]) == -1 || close(my_pipe[1]) == -1)
      {
        perror("smash error: close failed");
        return;
      }
      if (regular_pipe)
      {
        if (waitpid(pid_1,nullptr,WNOHANG) == -1)
        {
          perror("smash error: waitpid failed");
          return;      
        }      
      }
      if (waitpid(pid_2,nullptr,0) == -1)
      {
        perror("smash error: waitpid failed");
        return;      
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////BUILT_IN_COMMANDS/////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////


ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line,false)
{
  if (args_num > 1)
  {
    this->prompt = args[1];
  }
}

void ChangePromptCommand::execute()
{
  SmallShell& smash = SmallShell::getInstance();
  if (args_num == 1)
  {
    smash.setPrompt("smash");
  }
  else
  {
    smash.setPrompt(prompt);
  }
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()
{
  cout << "smash pid is " << SmallShell::getInstance().getSmashPid() << endl;
}


GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute()
{
  char buffer[COMMAND_MAX_LENGTH];
  if(getcwd(buffer,COMMAND_MAX_LENGTH) == nullptr)
  {      
    perror("smash error: getcwd failed");
    return;
  }
  cout << buffer << endl;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void ChangeDirCommand::execute()
{
  SmallShell& instance = SmallShell::getInstance();
  if (args_num == 1)
  {
    return;
  }
  else if (args_num > 2)
  {
    std::cerr << "smash error: cd: too many arguments" << endl;
    return;
  }
  else if (*args[1] == '-')
  {
    if (!instance.getLastPwd().compare(""))
    {
      std::cerr << "smash error: cd: OLDPWD not set" << endl;
      return;
    }
    char buffer[COMMAND_MAX_LENGTH];
    if(getcwd(buffer,COMMAND_MAX_LENGTH) == nullptr)
    {      
      perror("smash error: getcwd failed");
      return;
    }
    int check = chdir(instance.getLastPwd().c_str());
    if (check == -1 )
    {
      perror("smash error: chdir failed");
      return;
    }
    else if(!check)
    {
      instance.setLastPwd(buffer);
    }
  }
  else if (args_num == 2)
  {
    char buffer[COMMAND_MAX_LENGTH];
    if(getcwd(buffer,COMMAND_MAX_LENGTH) == nullptr)
    {      
      perror("smash error: getcwd failed");
      return;
    }
    int check = chdir(args[1]);
    if (check == -1 )
    {
      perror("smash error: chdir failed");
      return;
    }
    else if(!check)
    {
      instance.setLastPwd(buffer);
    }
  }
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList* jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute()
{
  this->jobs->printJobsList();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute()
{
  if (args_num > 2)
  {
    std::cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  else if (args_num > 1 && !isNumber(args[1]))
  {
    std::cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  else if (args_num > 1 && atoi(args[1]) < 1) 
  {
    std::cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  else
  {
    JobsList::JobEntry* job;
    if (args_num == 1)
    {
      if (jobs->getJobsList()->empty())
      {
        std::cerr << "smash error: fg: jobs list is empty" << endl;
        return;
      }
      job = jobs->getLastJob();
      job->setAsForeground();
      job->printJobEntryWithPid();
      if (waitpid(job->getPid(),nullptr,0) == -1)
      {
        perror("smash error: waitpid failed");
        return;
      } 
    }
    else if (args_num == 2)
    {
      job = jobs->getJobById(atoi(args[1]));
      if (!job)
      {
        std::cerr << "smash error: fg: job-id " << args[1] << " does not exist" << endl;
        return;
      }
      else 
      {
        job->setAsForeground();
        job->printJobEntryWithPid();
        if (waitpid(job->getPid(),nullptr,0) == -1)
        {
          perror("smash error: waitpid failed");
          return;
        } 
      }
    }
    this->jobs->removeFinishedJobs();
  }
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList* jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute()
{
  jobs->removeFinishedJobs();
  if (args_num == 1)
  {
    exit(0);
  }
  else
  {
    if (strcmp(args[1],"kill") == 0)
    {
      cout << "smash: sending SIGKILL signal to " << jobs->getJobsList()->size() << " jobs:" << endl;
      jobs->killAllJobs();
      exit(0);
    }
    else 
    {
      exit(0);
    }
  }
}

KillCommand::KillCommand(const char *cmd_line, JobsList* jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute()
{
  if (args_num != 3)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  else if ((args[1][0] != '-') || !isNumber(args[1]) || !isNumber(args[2]) || atoi(args[2]) < 1)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  else 
  {
    JobsList::JobEntry* job;
    job = jobs->getJobById(atoi(args[2]));
    if (!job)
    {
      cerr << "smash error: kill: job-id " << atoi(args[2]) << " does not exist" << endl;
      return;
    }
    job->sendSignal(abs(atoi(args[1])));
  }
}

aliasCommand::aliasCommand(const char *cmd_line,AliasList* alias_list) : BuiltInCommand(cmd_line),
                                                                          alias_list(alias_list) {}

bool isItBuiltIn(std::string string)
{
  if (!string.compare("showpid") || !string.compare("chprompt") || !string.compare("pwd") ||
      !string.compare("cd") || !string.compare("fg") || !string.compare("kill") ||
      !string.compare("quit") || !string.compare("alias") || !string.compare("unalias") ||
      !string.compare("getuser") || !string.compare("watch") || !string.compare("jobs") ||
      !string.compare("listdir"))
  {
    return true;
  }
  return false;
}

void aliasCommand::execute()
{
  string tmp = this->cmd_line;
  tmp = _trim(tmp);
  std::regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
  if (args_num == 1)
  {
    alias_list->printList();
  }
  else if (!std::regex_match(tmp,pattern))
  {
    cerr << "smash error: alias: invalid alias format" << endl;
    return;
  }
  else 
  {
    std::string name = tmp;
    size_t start = name.find_first_of(WHITESPACE);
    size_t end = name.find_first_of("=");
    name = name.substr(start+1,end-start-1);
    if (alias_list->aliasNameAlreadyExists(name) || isItBuiltIn(name))
    {
      cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
      return;
    }
    else
    {
      std::string alias_cmd_line = tmp;
      start = alias_cmd_line.find_first_of("'");
      end = alias_cmd_line.find_last_of("'");
      alias_cmd_line = alias_cmd_line.substr(start+1,end-start-1);
      alias_list->addAlias(alias_cmd_line,name);
    }
  }
}

unaliasCommand::unaliasCommand(const char *cmd_line,AliasList* alias_list) : BuiltInCommand(cmd_line),
                                                                             alias_list(alias_list) {}

void unaliasCommand::execute()
{
  if (args_num == 1)
  {
    cerr << "smash error: unalias: not enough arguments" << endl;
    return;
  }
  else 
  {
    for (int i = 1; i < args_num ; i++)
    {
      if (alias_list->aliasNameAlreadyExists(args[i]))
      {
        alias_list->removeAliasByName(args[i]);
      }
      else 
      {
        cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
        return;
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////SMALL_SHELL//////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


SmallShell::SmallShell() : prompt("smash"), last_pwd("")
{
  this->pid = getpid();
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
int _parseCommandLinePrivate(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = new char[s.length()+1];;
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

Command *SmallShell::CreateCommand(const char *cmd_line) 
{
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  std::string original_cmd_line = cmd_line;
  bool isAlias = false;
  if (AliasList::getInstance().aliasNameAlreadyExists(firstWord))
  {
    isAlias = true;
    char * copy_original_cmd_line = new char[string(original_cmd_line).length()+1];
    strcpy(copy_original_cmd_line,original_cmd_line.c_str());
    cmd_line = AliasList::getInstance().getAliasCmdLineByName(firstWord).c_str();
    char * copy_cmd_line = new char[string(cmd_line).length()+1];
    strcpy(copy_cmd_line,cmd_line);
    char* args[COMMAND_MAX_ARGS+1];
    int args_num = 0;
    args_num = _parseCommandLinePrivate(copy_original_cmd_line,args);
    std::string tmp_str = copy_cmd_line;
    for (int i = 1 ; i < args_num; i++)
    {
      tmp_str = tmp_str + " " + string(args[i]);
    }
    cmd_line = new char[tmp_str.length() + 1];
    strcpy((char*)cmd_line,tmp_str.c_str());
    cmd_s = _trim(string(cmd_line));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  }
  std::regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
  if (_isBackgroundComamnd(cmd_s.c_str()))
  {
      _removeBackgroundSign((char*)cmd_s.c_str());
      cmd_s = _trim(cmd_s);
  }
  if (cmd_s.find('>') != std::string::npos && !std::regex_match(cmd_s,pattern) && firstWord.compare("watch") != 0)
  {
    return new RedirectionCommand(cmd_line);
  }
  if (cmd_s.find('|') != std::string::npos && !std::regex_match(cmd_s,pattern) && firstWord.compare("watch") != 0)
  {
    return new PipeCommand(cmd_line);
  }
  if (firstWord.compare("chprompt&") == 0 || firstWord.compare("chprompt") == 0) 
  {
    return new ChangePromptCommand(cmd_line);
  }
  else if (firstWord.compare("showpid&") == 0 || firstWord.compare("showpid") == 0) 
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd&") == 0 || firstWord.compare("pwd") == 0) 
  {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("cd&") == 0 || firstWord.compare("cd") == 0) 
  {
    return new ChangeDirCommand(cmd_line);
  }
  else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0) 
  {
    return new JobsCommand(cmd_line,&JobsList::getInstance());
  }
  else if (firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0) 
  {
    return new ForegroundCommand(cmd_line,&JobsList::getInstance());
  }
  else if (firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0) 
  {
    return new QuitCommand(cmd_line,&JobsList::getInstance());
  }
  else if (firstWord.compare("kill") == 0 || firstWord.compare("kill&") == 0) 
  {
    return new KillCommand(cmd_line,&JobsList::getInstance());
  }
  else if (firstWord.compare("alias") == 0 || firstWord.compare("alias&") == 0) 
  {
    return new aliasCommand(cmd_line,&AliasList::getInstance());
  }
  else if (firstWord.compare("unalias") == 0 || firstWord.compare("unalias&") == 0) 
  {
    return new unaliasCommand(cmd_line,&AliasList::getInstance());
  }
  else if (firstWord.compare("listdir") == 0 || firstWord.compare("listdir&") == 0) 
  {
    return new ListDirCommand(cmd_line);
  }
  else if (firstWord.compare("getuser") == 0 || firstWord.compare("getuser&") == 0) 
  {
    return new GetUserCommand(cmd_line);
  }
  else if (firstWord.compare("watch") == 0 || firstWord.compare("watch&") == 0) 
  {
    return new WatchCommand(cmd_line);
  }
  else 
  {
    return new ExternalCommand(cmd_line,original_cmd_line,isAlias);
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) 
{
  JobsList::getInstance().removeFinishedJobs();
  Command* cmd = CreateCommand(cmd_line);
  if (!cmd)
  {
    return;
  }
  cmd->execute();
}

void SmallShell::setPrompt(const std::string & new_prompt)
{
  this->prompt = new_prompt;
}

std::string SmallShell::getPrompt()
{
  return this->prompt + "> ";
}

pid_t SmallShell::getSmashPid()
{
  return this->pid;
}

void SmallShell::setLastPwd(const std::string& last_pwd)
{
  this->last_pwd = last_pwd;
}

std::string SmallShell::getLastPwd()
{
  return this->last_pwd;
}


//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////ALIAS_ENTRY/////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////


AliasList::AliasEntry::AliasEntry(std::string cmd_line,std::string name) : cmd_line(cmd_line) , name(name){}

std::string AliasList::AliasEntry::getAliasEntryCmdLine()
{
  return this->cmd_line;
}

std::string AliasList::AliasEntry::getAliasEntryName()
{
  return this->name;
}

bool AliasList::AliasEntry::operator==(const AliasEntry &alias_entry) const
{
  return this->name.compare(alias_entry.name) == 0;
}

bool AliasList::AliasEntry::operator>(const AliasEntry &alias_entry) const
{
  return this->name.compare(alias_entry.name) > 0;
}

bool AliasList::AliasEntry::operator<(const AliasEntry &alias_entry) const
{
  return this->name.compare(alias_entry.name) < 0;
}

void AliasList::AliasEntry::printAliasEntry()
{
  cout << name << "='" << cmd_line << "'" << endl;
}


///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////ALIAS_LIST///////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////


AliasList::AliasList() : alias_list(std::vector<AliasEntry>()) {}

void AliasList::printList()
{
  for (AliasEntry alias_entry : alias_list)
  {
    alias_entry.printAliasEntry();
  }
}

void AliasList::clearList()
{
  if (alias_list.size() != 0)
  {
    alias_list.clear();
  }
}

bool AliasList::aliasNameAlreadyExists(std::string name)
{
  for (AliasEntry alias_entry : alias_list) 
  {
    if (alias_entry.getAliasEntryName().compare(name) == 0 )
    {
      return true;
    }
  }
  return false;
}

void AliasList::removeAliasByName(std::string name)
{
  for (AliasEntry& alias_entry : alias_list) 
  {
    if (alias_entry.getAliasEntryName().compare(name) == 0 )
    {
      alias_list.erase(std::vector<AliasEntry>::const_iterator(&alias_entry));
      return;
    }
  }
}

std::string AliasList::getAliasCmdLineByName(std::string name)
{
  for (AliasEntry alias_entry : alias_list) 
  {
    if (alias_entry.getAliasEntryName().compare(name) == 0 )
    {
      return alias_entry.getAliasEntryCmdLine();
    }
  }
  return nullptr;
}

void AliasList::addAlias(std::string alias_cmd_line,std::string name)
{
  alias_list.push_back(AliasEntry(alias_cmd_line,name));
}


///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////JOP_ENTRY////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////


JobsList::JobEntry::JobEntry(std::string cmd_line, int job_id, int pid, bool background):
    cmd_line(cmd_line),id(job_id),pid(pid),background(background){}

bool JobsList::JobEntry::operator==(const JobEntry &job) const
{
  return id == job.id;
}

bool JobsList::JobEntry::operator>(const JobEntry &job) const
{
  return id > job.id;
}

bool JobsList::JobEntry::operator<(const JobEntry &job) const
{
  return id < job.id;
}

int JobsList::JobEntry::getId()
{
  return this->id;
}

pid_t JobsList::JobEntry::getPid()
{
  return this->pid;
}

void JobsList::JobEntry::printJobEntryWithId()
{
  cout << "[" << this->id << "] " << this->cmd_line << endl;
}

void JobsList::JobEntry::printJobEntryWithPid()
{
  cout << this->cmd_line << " " << this->pid << endl;
}

void JobsList::JobEntry::killJob()
{
  if (kill(pid,SIGKILL) == -1)
  {
    perror("smash error: kill failed");
  }
  cout << this->pid << ": " << this->cmd_line << endl;
}

void JobsList::JobEntry::sendSignal(int signal)
{
  if (kill(pid,signal) == -1)
  {
    perror("smash error: kill failed");
  }
  cout << "signal number " << signal << " was sent to pid "<< pid << endl; 
}

bool JobsList::JobEntry::getBackground()
{
  return this->background;
}

void JobsList::JobEntry::setAsForeground()
{
  this->background = false;
}


////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////JOPS_LIST/////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


JobsList::JobsList() : jobs_list(std::vector<JobEntry>()), max_id_in_list(0) {}

void JobsList::addJob(std::string cmd_line, pid_t pid,bool background)
{
  removeFinishedJobs();
  this->jobs_list.push_back(JobEntry( cmd_line, max_id_in_list + 1, pid, background));
  this->max_id_in_list++;
}

void JobsList::removeFinishedJobs()
{
  for (JobEntry job_entry : jobs_list)
  {
    pid_t pid = waitpid(job_entry.getPid(),nullptr,WNOHANG);
    if (pid != 0)
    {
      this->removeJobById(job_entry.getId());
    }
  }
}

void JobsList::updateMaxId()
{
  int tmp_max_id = NO_JOBS_ID;
  for (JobEntry job_entry : jobs_list)
  {
    if (job_entry.getId() > tmp_max_id)
    {
      tmp_max_id = job_entry.getId();
    }
  }
  this->max_id_in_list = tmp_max_id;
}

void JobsList::removeJobById(int jobId)
{
  for (JobEntry& job_entry : jobs_list) 
  {
    if (job_entry.getId() == jobId)
    {
      jobs_list.erase(std::vector<JobEntry>::const_iterator(&job_entry));
      break;
    }
  }
  this->updateMaxId();
  removeFinishedJobs();
}

void JobsList::printJobsList()
{
  removeFinishedJobs();
  for (JobEntry job_entry : jobs_list)
  {
    job_entry.printJobEntryWithId();
  }
}

JobsList::JobEntry * JobsList::getLastJob()
{
  removeFinishedJobs();
  if (!max_id_in_list)
  {
    return nullptr;
  }
  return &this->jobs_list.back();
}

JobsList::JobEntry * JobsList::getJobById(int jobId)
{
  removeFinishedJobs();
  for (JobEntry& job_entry : jobs_list)
  {
    if (job_entry.getId() == jobId)
    {
      return &job_entry;
    }
  }
  return nullptr;
}

std::vector<JobsList::JobEntry> * JobsList::getJobsList()
{
  return &this->jobs_list;
}

void JobsList::killAllJobs()
{
  removeFinishedJobs();
  for (JobEntry job_entry : jobs_list)
  {
    job_entry.killJob();
  }
}

bool JobsList::isJobExistsByPid(pid_t pid)
{
  removeFinishedJobs();
  for (JobEntry job_entry : jobs_list)
  {
    if (job_entry.getPid() == pid)
    {
      return true;
    }
  }
  return false;
}

JobsList::JobEntry *JobsList::getJobInForeground()
{
  removeFinishedJobs();
  for (JobEntry& job_entry : jobs_list)
  {
    if (job_entry.getBackground() == false)
    {
      return &job_entry;
    }
  }
  return nullptr;
}