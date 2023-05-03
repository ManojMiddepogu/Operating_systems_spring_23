#include <bits/stdc++.h>
using namespace std;
typedef enum
{
    BLOCKED,
    CREATED,
    DONE,
    READY,
    PREMPTION,
    RUNNING,
} process_state;

struct Process
{
    process_state state;
    int at;    // arrival time
    int tc;    // total cpu needed
    int cb;    // cpu_burst
    int io;    // io_burst
    int ft;    // finish time
    int it;    // IO time
    int ct;    // CPU time
    int remCb; // remaining CPU Burst
    int sb;    // start of CPU burst
    int pid;
    int priority;
    int dynamic;
    Process() : at(0), tc(0), cb(0), io(0), ft(0), it(0), ct(0), remCb(0), dynamic(0), sb(0) {}
};

struct Event
{
    int timeId;          // timeId of event
    int pid;             // process id
    int ct;              // created time
    process_state state; // state of the event to go to
    Event() : timeId(0), pid(0), ct(0), state(CREATED) {}
};

class schedule
{
public:
    vector<Process *> runq;
    int maxprios;
    virtual void addProc(Process *p)
    {
    }
    virtual Process *getProc()
    {
        return nullptr;
    }
};

class FCFS : public schedule
{
    queue<Process *> frun;
    Process *getProc()
    {
        Process *proc{nullptr};
        if (!frun.empty())
        {
            proc = frun.front();
            frun.pop();
        }
        return proc;
    }
    void addProc(Process *p)
    {
        frun.push(p);
    }
};

class LCFS : public schedule
{
    stack<Process *> srun;
    Process *getProc()
    {
        Process *proc{nullptr};
        if (!srun.empty())
        {
            proc = srun.top();
            srun.pop();
        }
        return proc;
    }
    void addProc(Process *p)
    {
        srun.push(p);
    }
};

class SRT : public schedule
{
    deque<Process *> lrun;
    Process *getProc()
    {
        Process *proc{nullptr};
        if (!lrun.empty())
        {
            proc = *lrun.begin();
            lrun.pop_front();
        }
        return proc;
    }
    void addProc(Process *p)
    {
        if (lrun.empty())
        {
            lrun.push_back(p);
            return;
        }
        auto it = lrun.begin();
        for (; it != lrun.end(); it++)
        {
            Process *temp = *it;
            if ((p->tc - p->ct) < (temp->tc - temp->ct))
            {
                break;
            }
        }
        lrun.insert(it, p);
    }
};

class PRIO : public schedule
{
public:
    queue<Process *> *aq, *dq;
    PRIO(int maxPrios)
    {
        aq = new queue<Process *>[maxPrios];
        dq = new queue<Process *>[maxPrios];
    }
    Process *getProc()
    {
        Process *p{nullptr};
        int i = maxprios - 1;
        for (; i >= 0; i--)
        {
            if (aq[i].empty())
                continue;
            else
            {
                p = aq[i].front();
                aq[i].pop();
                return p;
            }
        }
        bool emp = false;
        for (int i = 0; i < maxprios; i++)
        {
            emp = emp || !(dq[i].empty());
            if (emp)
                break;
        }
        if (!emp)
            return nullptr;
        swap(aq, dq);
        return getProc();
    }
    void addProc(Process *p)
    {
        if (p->dynamic >= 0)
        {
            aq[p->dynamic].push(p);
        }
        else
        {
            dq[p->priority - 1].push(p);
            p->dynamic = p->priority - 1;
        }
    }
};

vector<int> randvals;
vector<Process> myProcess;
list<Event> des;
int ofs = -1;
int des_counter = -1;
string inputfile = "";
string randfile = "";
schedule *sch;
int quantum = 1e6;
int maxprios = 4;
int iofinish = 0;
int time_iobusy = 0;
int time_cpubusy = 0;
int finishtime = 0;
string scheduler = "";
bool doesPrempt = 0;
bool preprio = 0;

int myrandom(int burst)
{
    if (ofs == randvals.size() - 1)
    {
        ofs = 0;
    }
    else
        ofs++;
    return 1 + (randvals[ofs] % burst);
}

void push_des(list<Event>::iterator from, Event &e)
{
    auto it = from;
    for (; it != des.end(); it++)
    {
        if (it->timeId > e.timeId)
        {
            break;
        }
    }
    des.insert(it, e);
}

void readProcess(string filename)
{
    ifstream f;
    f.open(filename);
    int proc_count = 0;
    string temp;
    while (f >> temp)
    {
        Process p = {};
        p.pid = proc_count++;
        p.at = stoi(temp);
        f >> temp;
        p.tc = stoi(temp);
        f >> temp;
        p.cb = stoi(temp);
        f >> temp;
        p.io = stoi(temp);
        myProcess.push_back(p);
    }
    f.close();
}

void readRandomNumbers(string filename)
{
    ifstream f;
    f.open(filename);
    string temp;
    f >> temp;
    while (f >> temp)
    {
        randvals.push_back(stoi(temp));
    }
    f.close();
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strlen(argv[i]) > 2 && argv[i][0] == '-' && argv[i][1] == 's')
        {
            if (argv[i][2] == 'F')
            {
                sch = new FCFS();
                cout << "FCFS\n";
            }
            else if (argv[i][2] == 'L')
            {
                sch = new LCFS();
                cout << "LCFS\n";
            }
            else if (argv[i][2] == 'S')
            {
                sch = new SRT();
                cout << "SRTF\n";
            }
            else if (argv[i][2] == 'R')
            {
                sch = new FCFS();
                string temp = "";
                for (int j = 3; j < strlen(argv[i]); j++)
                {
                    temp += argv[i][j];
                }
                quantum = stoi(temp);
                cout << "RR " << quantum << endl;
            }
            else if (argv[i][2] == 'P' || 'E')
            {
                string temp = "";
                int j = 3;
                for (; argv[i][j] != ':'; j++)
                {
                    temp += argv[i][j];
                }
                quantum = stoi(temp);
                j++;
                temp = "";
                for (; j < strlen(argv[i]); j++)
                {
                    temp += argv[i][j];
                }
                if (temp != "")
                {
                    maxprios = stoi(temp);
                }
                sch = new PRIO(maxprios);
                sch->maxprios = maxprios;
                doesPrempt = true;
                if (argv[i][2] == 'E')
                {
                    preprio = true;
                    cout << "PREPRIO " << quantum << endl;
                }
                else
                {
                    cout << "PRIO " << quantum << endl;
                }
            }
        }
        if (argv[i][0] != '-' && inputfile == "")
        {
            for (int j = 0; j < strlen(argv[i]); j++)
            {
                inputfile += argv[i][j];
            }
            readProcess(inputfile);
            continue;
        }
        if (argv[i][0] != '-' && randfile == "")
        {
            for (int j = 0; j < strlen(argv[i]); j++)
            {
                randfile += argv[i][j];
            }
            readRandomNumbers(randfile);
        }
    }
    for (int i = 0; i < myProcess.size(); i++)
    {
        myProcess[i].priority = myrandom(maxprios);
        myProcess[i].dynamic = myProcess[i].priority - 1;
        Event e = {};
        e.state = READY;
        e.ct = myProcess[i].at;
        e.pid = i;
        e.timeId = myProcess[i].at;
        push_des(des.begin(), e);
    }

    bool call_scheduler = 0;
    int current_time = 0;
    Process *current_process{nullptr};
    auto desit = des.begin();
    for (; desit != des.end(); desit++)
    {
        des_counter++;
        Event e = *desit;
        current_time = e.timeId;
        if (e.state == READY)
        {
            call_scheduler = 1;
            sch->addProc(&myProcess[e.pid]);

            if (preprio && doesPrempt && current_process != nullptr && current_process->dynamic < myProcess[e.pid].dynamic)
            {

                int cid = current_process->pid;
                auto it = desit;
                for (; it != des.end(); it++)
                {
                    if (it->pid == cid)
                    {
                        break;
                    }
                }
                if (!(it->timeId == current_time && (current_process->tc - current_process->ct == 0)))
                {
                    int actual = it->timeId - current_process->sb;
                    int cut = current_time - current_process->sb;
                    int loss = it->timeId - current_time;
                    if (loss != 0)
                    {
                        current_process->remCb += loss;
                        current_process->ct -= loss;
                        Event ne;
                        ne.pid = cid;
                        ne.state = PREMPTION;
                        ne.ct = current_time;
                        ne.timeId = current_time;
                        des.erase(it);
                        push_des(desit, ne);
                    }
                }
            }
        }
        else if (e.state == RUNNING)
        {
            current_process = &myProcess[e.pid];
            int burst;
            if (myProcess[e.pid].remCb > 0)
            {
                burst = myProcess[e.pid].remCb;
            }
            else
            {
                burst = myrandom(myProcess[e.pid].cb);
            }
            if (burst > quantum)
            {
                myProcess[e.pid].ct += quantum;
                myProcess[e.pid].remCb = (burst - quantum);
                myProcess[e.pid].sb = current_time;
                Event ne;
                ne.pid = e.pid;
                ne.ct = current_time;
                ne.timeId = current_time + quantum;
                if (myProcess[e.pid].ct >= myProcess[e.pid].tc)
                {
                    ne.timeId = current_time + (myProcess[e.pid].tc - myProcess[e.pid].ct + quantum);
                    myProcess[e.pid].ct = myProcess[e.pid].tc;
                    ne.state = DONE;
                }
                else
                {
                    ne.state = PREMPTION;
                }
                push_des(desit, ne);
            }
            else if (burst <= quantum && burst + myProcess[e.pid].ct < myProcess[e.pid].tc)
            {
                myProcess[e.pid].remCb = 0;
                myProcess[e.pid].ct += burst;
                myProcess[e.pid].sb = current_time;
                Event ne;
                ne.pid = e.pid;
                ne.state = BLOCKED;
                ne.ct = current_time;
                ne.timeId = current_time + burst;
                push_des(desit, ne);
            }
            else if (burst <= quantum && burst + myProcess[e.pid].ct >= myProcess[e.pid].tc)
            {
                myProcess[e.pid].remCb = 0;
                int rem = myProcess[e.pid].tc - myProcess[e.pid].ct;
                myProcess[e.pid].ct = myProcess[e.pid].tc;
                myProcess[e.pid].sb = current_time;
                Event ne;
                ne.pid = e.pid;
                ne.state = DONE;
                ne.timeId = current_time + rem;
                ne.ct = current_time;
                push_des(desit, ne);
            }
        }
        else if (e.state == BLOCKED)
        {
            int burst = myrandom(myProcess[e.pid].io);
            myProcess[e.pid].it += burst;
            Event ne;
            ne.pid = e.pid;
            ne.state = READY;
            ne.timeId = current_time + burst;
            ne.ct = current_time;
            push_des(desit, ne);
            if (current_time >= iofinish)
            {
                iofinish = ne.timeId;
                time_iobusy += burst;
            }
            else if (ne.timeId > iofinish)
            {
                time_iobusy += (ne.timeId - iofinish);
                iofinish = ne.timeId;
            }
            call_scheduler = 1;
            myProcess[e.pid].dynamic = myProcess[e.pid].priority - 1;
            current_process = nullptr;
        }
        else if (e.state == PREMPTION)
        {
            if (doesPrempt)
            {
                myProcess[e.pid].dynamic -= 1;
            }
            sch->addProc(&myProcess[e.pid]);
            call_scheduler = 1;
            current_process = nullptr;
        }
        else if (e.state == DONE)
        {
            myProcess[e.pid].ft = current_time;
            finishtime = current_time;
            call_scheduler = 1;
            current_process = nullptr;
        }
        if (call_scheduler)
        {
            auto it = desit;
            it++;
            if (des_counter != des.size() - 1 && it->timeId == current_time)
            {
                continue;
            }
            call_scheduler = 0;
            if (current_process == nullptr)
            {
                current_process = sch->getProc();
                if (current_process == nullptr)
                {
                    continue;
                }
                Event ne;
                ne.timeId = current_time;
                ne.ct = current_time;
                ne.pid = current_process->pid;
                ne.state = RUNNING;
                push_des(desit, ne);
                current_process = nullptr;
            }
        }
    }

    int tt = 0, wt = 0;
    for (int i = 0; i < myProcess.size(); i++)
    {
        tt += (myProcess[i].ft - myProcess[i].at);
        int curw = (myProcess[i].ft - myProcess[i].at - myProcess[i].it - myProcess[i].tc);
        wt += curw;
        time_cpubusy += myProcess[i].tc;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
               myProcess[i].pid, myProcess[i].at, myProcess[i].tc, myProcess[i].cb,
               myProcess[i].io, myProcess[i].priority, myProcess[i].ft, myProcess[i].ft - myProcess[i].at, myProcess[i].it, curw);
    }
    double cpu_util = 100.0 * (time_cpubusy / (double)finishtime);
    double io_util = 100.0 * (time_iobusy / (double)finishtime);
    double avg_tt = (tt / (double)myProcess.size());
    double avg_wt = (wt / (double)myProcess.size());
    double throughput = 100.0 * (myProcess.size() / (double)finishtime);
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
           finishtime, cpu_util, io_util, avg_tt, avg_wt, throughput);
}