#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

#define NO_JOBS_ID 0

class SmallShell;
class JobsList;
class AliasList;


/*################################################################################################
##############################################COMMAND#############################################
##################################################################################################*/


class Command {
protected:
    char* args[COMMAND_MAX_ARGS + 1];
    int args_num;
    char* cmd_line;
    bool isBackground;
public:
    Command(const char *cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    char* getCmdLine();
};


/*################################################################################################
###########################################BUILT_IN_COMMAND#######################################
##################################################################################################*/


class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line,bool remove_bachground_sing = true);
    virtual ~BuiltInCommand() = default;
};


/*################################################################################################
###########################################EXTERNAL_COMMAND#######################################
##################################################################################################*/


class ExternalCommand : public Command {
    std::string original_cmd_line;
    bool isAlias;
public:
    ExternalCommand(const char *cmd_line,std::string original_cmd_line, bool isAlias);
    virtual ~ExternalCommand() = default;
    void execute() override;
};


/*################################################################################################
##########################################REDIRECTION_COMMAND#####################################
##################################################################################################*/


class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const char *cmd_line);
    virtual ~RedirectionCommand() = default;
    void execute() override;
};


/*################################################################################################
##########################################LIST_DIR_COMMAND########################################
##################################################################################################*/


class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char *cmd_line);
    virtual ~ListDirCommand() = default;
    void execute() override;
};


/*################################################################################################
##########################################GET_USER_COMMAND########################################
##################################################################################################*/


class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *cmd_line);
    virtual ~GetUserCommand() = default;
    void execute() override;
};

/*################################################################################################
##########################################BOUNS_COMMANDS##########################################
##################################################################################################*/

class PipeCommand : public Command {
public:
    PipeCommand(const char *cmd_line);
    virtual ~PipeCommand() = default;
    void execute() override;
};

class WatchCommand : public Command {
public:
    WatchCommand(const char *cmd_line);
    virtual ~WatchCommand() = default;
    void execute() override;
};


/*################################################################################################
###########################################BUILT_IN-COMMANDS######################################
##################################################################################################*/


class ChangePromptCommand : public BuiltInCommand {
    std::string prompt;
public:
    ChangePromptCommand(const char *cmd_line);
    virtual ~ChangePromptCommand() = default;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);
    virtual ~ShowPidCommand() = default;
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);
    virtual ~GetCurrDirCommand() = default;
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line);
    virtual ~ChangeDirCommand() = default;
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char *cmd_line, JobsList* jobs);
    virtual ~JobsCommand() = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);
    virtual ~ForegroundCommand() = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);
    virtual ~QuitCommand() = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);
    virtual ~KillCommand() = default;
    void execute() override;
};

class aliasCommand : public BuiltInCommand {
    AliasList * alias_list;
public:
    aliasCommand(const char *cmd_line,AliasList* alias_list);
    virtual ~aliasCommand() = default;
    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
    AliasList * alias_list;
public:
    unaliasCommand(const char *cmd_line,AliasList* alias_list);
    virtual ~unaliasCommand() = default;
    void execute() override;
};


/*################################################################################################
###########################################ALIAS_LIST_&_ENTRY#####################################
##################################################################################################*/


class AliasList {
public:
        class AliasEntry {
            std::string cmd_line;
            std::string name;
        public:
            AliasEntry(std::string cmd_line,std::string name);
            AliasEntry(AliasEntry const&) = default;
            ~AliasEntry() = default;
            AliasEntry& operator=(AliasEntry const&) = default;
            bool operator==(const AliasEntry &AliasEntry) const;
            bool operator>(const AliasEntry &AliasEntry) const;
            bool operator<(const AliasEntry &AliasEntry) const;           
            std::string getAliasEntryName();
            std::string getAliasEntryCmdLine();
            void printAliasEntry();
        };
private:
    std::vector<AliasEntry> alias_list;
public:
    AliasList();
    ~AliasList() = default;
    static AliasList &getInstance()
    {
        static AliasList instance;
        return instance;
    }
    void clearList();
    void printList();
    bool aliasNameAlreadyExists(std::string name);
    void removeAliasByName(std::string name);
    std::string getAliasCmdLineByName(std::string name);
    void addAlias(std::string alias_cmd_line, std::string name);
};


/*################################################################################################
###########################################JOBS_LIST_&_ENTRY######################################
##################################################################################################*/


class JobsList {
public:
        class JobEntry {
            std::string cmd_line;
            int id;
            pid_t pid;
            bool background;
        public:
            JobEntry(std::string cmd_line, int job_id, int pid, bool background);
            JobEntry(JobEntry const&) = default;
            ~JobEntry() = default;
            JobEntry& operator=(JobEntry const&) = default;
            bool operator==(const JobEntry &job_entry) const;
            bool operator>(const JobEntry &job_entry) const;
            bool operator<(const JobEntry &job_entry) const;           
            int getId();
            pid_t getPid();
            void printJobEntryWithId();
            void printJobEntryWithPid();
            void killJob();
            void sendSignal(int signal);
            bool getBackground();
            void setAsForeground();
        };
private:
    std::vector<JobEntry> jobs_list;
    int max_id_in_list;
public:
    JobsList();
    ~JobsList() = default;
    static JobsList &getInstance()
    {
        static JobsList instance;
        return instance;
    }
    void addJob(std::string cmd_line, pid_t pid,bool background = true);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry *getJobById(int jobId);
    void removeJobById(int jobId);
    JobEntry *getLastJob();
    void updateMaxId();
    std::vector<JobEntry> * getJobsList();
    bool isJobExistsByPid(pid_t pid);
    JobEntry *getJobInForeground();
};


/*################################################################################################
###########################################SMALL_SHELL############################################
##################################################################################################*/


class SmallShell {
private:
    std::string prompt;
    pid_t pid;
    SmallShell();
    std::string last_pwd;
public:
    Command *CreateCommand(const char *cmd_line);
    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell() = default;
    void executeCommand(const char *cmd_line);
    void setPrompt(const std::string & new_prompt);
    std::string getPrompt();
    pid_t getSmashPid();
    void setLastPwd(const std::string& last_pwd);
    std::string getLastPwd();
};


#endif //SMASH_COMMAND_H_
