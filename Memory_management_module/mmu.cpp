#include <bits/stdc++.h>
using namespace std;
typedef long long int ll;

int max_frames;
int cpid = -1;
ll cost_maps = 350;
ll cost_unmaps = 410;
ll cost_ins = 3200;
ll cost_outs = 2750;
ll cost_fins = 2350;
ll cost_fouts = 2800;
ll cost_zeros = 150;
ll cost_segv = 440;
ll cost_segprot = 410;
ll cost_rw = 1;
ll cost_context_switch = 130;
ll cost_process_exit = 1230;
bool option_O = false;
bool option_P = false;
bool option_F = false;
bool option_S = false;
ll inst_count = 0;
ll ctx_switches = 0;
ll process_exits = 0;
ll cost = 0;

struct vma
{
    int start_vpage;
    int end_vpage;
    int write_protected;
    int file_mapped;
};

struct pte
{
    bool present : 1;
    bool referenced : 1;
    bool modified : 1;
    bool write_protected : 1;
    bool pagedout : 1;
    bool file_mapped : 1;
    bool init : 1;
    unsigned int frame : 25;
    pte() : present(0), referenced(0), modified(0), write_protected(0), pagedout(0), init(0), frame(0) {}
};

struct frame_t
{
    int pid;
    int vpage;
    int allocated;
    ll age;
    int id;
    int instr;
    frame_t() {}
    frame_t(int frame_id) : id(frame_id), allocated(0), pid(-1), vpage(-1) {}
};
vector<frame_t> vf;
deque<int> freepool;

struct process
{
    vector<vma> vvma;
    vector<pte> vpte;
    int pid;
    int unmaps;
    int maps;
    int ins;
    int outs;
    int fins;
    int fouts;
    int zeros;
    int segv;
    int segprot;
    process() : pid(-1), unmaps(0), maps(0), ins(0), outs(0), fins(0), fouts(0), zeros(0), segv(0), segprot(0)
    {
        vpte.resize(64);
    }
};
vector<process> vp;

vector<int> randvals;
int ofs = -1;
int myrandom(int burst)
{
    if (ofs == randvals.size() - 1)
    {
        ofs = 0;
    }
    else
        ofs++;
    return (randvals[ofs] % burst);
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

class Pager
{
public:
    virtual int select_victim_frame() = 0; // virtual base class
};

class Fifo : public Pager
{
public:
    int hand;
    int select_victim_frame()
    {
        hand = hand % max_frames;
        return hand++;
    }
    Fifo() : hand(0) {}
};

class Clock : public Pager
{
public:
    int hand = 0;
    int select_victim_frame()
    {
        hand = hand % max_frames;
        for (int i = 0; i < 2 * max_frames; i++)
        {
            int pid = vf[hand].pid;
            int vpage = vf[hand].vpage;
            pte &p = vp[pid].vpte[vpage];
            if (p.referenced)
            {
                p.referenced = 0;
                hand = (hand + 1) % max_frames;
            }
            else
            {
                return hand++;
            }
        }
        return -1;
    }
};

class Random : public Pager
{
public:
    int select_victim_frame()
    {
        return myrandom(max_frames);
    }
};

class NRU : public Pager
{
public:
    int hand;
    int period;
    int select_victim_frame()
    {
        int start = hand;
        int levels[4] = {-1, -1, -1, -1};
        while (true)
        {
            int pid = vf[hand].pid;
            int vpage = vf[hand].vpage;
            pte &p = vp[pid].vpte[vpage];
            int level = 2 * p.referenced + p.modified;
            if (levels[level] == -1)
            {
                levels[level] = hand;
            }
            if (inst_count - 1 - period >= 50)
            {
                p.referenced = 0;
            }
            else
            {
                if (levels[0] != -1)
                {
                    int ans = levels[0];
                    hand = (ans + 1) % max_frames;
                    return ans;
                }
            }
            hand = (hand + 1) % max_frames;
            if (hand == start)
                break;
        }
        if (inst_count - 1 - period >= 50)
        {
            period = inst_count - 1;
        }
        for (int i = 0; i < 4; i++)
        {
            if (levels[i] != -1)
            {
                hand = (levels[i] + 1) % max_frames;
                return levels[i];
            }
        }
        return -1;
    }
    NRU() : hand(0), period(-1) {}
};

class Aging : public Pager
{
public:
    int hand;
    int select_victim_frame()
    {
        int mini = hand;
        int start = hand;
        while (true)
        {
            int pid = vf[hand].pid;
            int vpage = vf[hand].vpage;
            pte &p = vp[pid].vpte[vpage];
            vf[hand].age >>= 1;
            if (p.referenced)
            {
                vf[hand].age |= (0x80000000);
                p.referenced = 0;
            }
            if (vf[hand].age < vf[mini].age)
            {
                mini = hand;
            }
            hand = (hand + 1) % max_frames;
            if (hand == start)
                break;
        }
        hand = (mini + 1) % max_frames;
        return mini;
    }
    Aging() : hand(0) {}
};

class WS : public Pager
{
public:
    int hand;
    int select_victim_frame()
    {
        int mini = hand;
        int start = hand;
        while (true)
        {
            // cout<<"hand "<<hand<<" instr "<<vf[hand].instr<<" "<<inst_count-1<<endl;
            int pid = vf[hand].pid;
            int vpage = vf[hand].vpage;
            pte &p = vp[pid].vpte[vpage];
            if (p.referenced)
            {
                vf[hand].instr = inst_count - 1;
                p.referenced = 0;
            }
            else
            {
                if (inst_count - 1 - vf[hand].instr >= 50)
                {
                    int ans = hand;
                    hand = (hand + 1) % max_frames;
                    return ans;
                }
                if (vf[hand].instr < vf[mini].instr)
                {
                    mini = hand;
                }
            }
            hand = (hand + 1) % max_frames;
            if (hand == start)
                break;
        }
        hand = (mini + 1) % max_frames;
        return mini;
    }
    WS() : hand(0) {}
};
Pager *main_pager;

// -------------

int allocate_frame_from_free_list()
{
    if (!freepool.empty())
    {
        int f = freepool.front();
        freepool.pop_front();
        return f;
    }
    return -1;
}

int get_frame()
{
    int frame_id = allocate_frame_from_free_list();
    if (frame_id == -1)
    {
        frame_id = main_pager->select_victim_frame();
        frame_t f = vf[frame_id];
        int prev_process = f.pid;
        int index = f.vpage;
        vp[prev_process].unmaps++;
        cost += cost_unmaps;
        if (option_O)
        {
            cout << " UNMAP " << prev_process << ":" << index << "\n";
        }
        pte &p = vp[prev_process].vpte[index];
        p.present = 0;
        if (p.modified)
        {
            if (p.file_mapped)
            {
                vp[prev_process].fouts++;
                cost += cost_fouts;
                if (option_O)
                {
                    cout << " FOUT\n";
                }
            }
            else
            {
                p.pagedout = 1;
                vp[prev_process].outs++;
                cost += cost_outs;
                if (option_O)
                {
                    cout << " OUT\n";
                }
            }
            p.modified = 0;
        }
    }
    return frame_id;
}

void pagefault(int index)
{
    pte &p = vp[cpid].vpte[index];
    if (!p.init)
    {
        bool found = false;
        // cout << vp[id].vvma.size() << endl;
        for (int i = 0; i < vp[cpid].vvma.size(); i++)
        {
            // cout << index << " " << vp[id].vvma[i].start_vpage << " " << vp[id].vvma[i].end_vpage << "\n";
            if (index >= vp[cpid].vvma[i].start_vpage && index <= vp[cpid].vvma[i].end_vpage)
            {
                p.init = 1;
                p.file_mapped = vp[cpid].vvma[i].file_mapped;
                p.write_protected = vp[cpid].vvma[i].write_protected;
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (option_O)
            {
                cout << " SEGV\n";
            }
            cost += cost_segv;
            vp[cpid].segv++;
            return;
        }
    }
    vp[cpid].vpte[index].referenced = 1;
    vp[cpid].vpte[index].init = 1;
    vp[cpid].vpte[index].frame = get_frame();
    frame_t &f = vf[vp[cpid].vpte[index].frame];
    f.pid = cpid;
    f.vpage = index;
    f.allocated = 1;
    vp[cpid].vpte[index].present = 1;
    if (vp[cpid].vpte[index].file_mapped)
    {
        vp[cpid].fins++;
        cost += cost_fins;
        if (option_O)
        {
            cout << " FIN\n";
        }
    }
    if (vp[cpid].vpte[index].pagedout)
    {
        vp[cpid].ins++;
        cost += cost_ins;
        if (option_O)
        {
            cout << " IN\n";
        }
    }
    if (!vp[cpid].vpte[index].file_mapped && !vp[cpid].vpte[index].pagedout)
    {
        vp[cpid].zeros++;
        cost += cost_zeros;
        if (option_O)
        {
            cout << " ZERO\n";
        }
    }
    vp[cpid].maps++;
    cost += cost_maps;
    if (option_O)
    {
        cout << " MAP " << vp[cpid].vpte[index].frame << "\n";
    }
    vf[vp[cpid].vpte[index].frame].age = 0;
    vf[vp[cpid].vpte[index].frame].instr = inst_count - 1;
}

void readInput(string filename)
{
    ifstream infile(filename);
    string input;
    while (getline(infile, input))
    {
        if (input[0] == '#')
            continue;

        if (input[0] >= '0' && input[0] <= '9')
        {
            for (int i = 0; i < stoi(input); i++)
            {
                process p;
                p.pid = i;
                string nvma;
                while (getline(infile, nvma))
                {
                    if (nvma[0] != '#')
                        break;
                }
                for (int j = 0; j < stoi(nvma); j++)
                {
                    string vma_i;
                    getline(infile, vma_i);
                    istringstream temp(vma_i);
                    vma v;
                    temp >> v.start_vpage >> v.end_vpage >> v.write_protected >> v.file_mapped;
                    // cout << v.start_vpage << " " << v.end_vpage << "\n";
                    p.vvma.push_back(v);
                }
                vp.push_back(p);
            }
        }
        else
        {
            inst_count++;
            string op, value;
            istringstream is(input);
            if (input[0] == 'e')
            {
                process_exits++;
                cost += cost_process_exit;
                if (option_O)
                {
                    cout << inst_count - 1 << ": ==> e " << cpid << "\n";
                    cout << "EXIT current process " << cpid << "\n";
                }
                for (int i = 0; i < 64; i++)
                {
                    pte &p = vp[cpid].vpte[i];
                    if (p.present)
                    {
                        vp[cpid].unmaps++;
                        cost += cost_unmaps;
                        if (option_O)
                        {
                            cout << " UNMAP " << cpid << ":" << i << "\n";
                        }
                        p.present = 0;
                        int frame = p.frame;
                        vf[frame].allocated = -1;
                        vf[frame].pid = -1;
                        vf[frame].vpage = -1;
                        freepool.push_back(frame);
                        if (p.modified && p.file_mapped)
                        {
                            vp[cpid].fouts++;
                            cost += cost_fouts;
                            if (option_O)
                            {
                                cout << " FOUT\n";
                            }
                        }
                    }
                    p.pagedout = 0;
                }
            }
            else
            {
                is >> op >> value;
                if (option_O)
                {
                    cout << inst_count - 1 << ": ==> " << op << " " << value << "\n";
                }
                int index = stoi(value);
                if (op == "c")
                {
                    cpid = index;
                    ctx_switches++;
                    cost += cost_context_switch;
                    continue;
                }
                pte &p = vp[cpid].vpte[index];
                p.referenced = 1;
                cost += cost_rw;
                if (!p.present)
                {
                    pagefault(index);
                }
                if (op == "w")
                {
                    if (p.write_protected)
                    {
                        cost += cost_segprot;
                        vp[cpid].segprot++;
                        if (option_O)
                        {
                            cout << " SEGPROT\n";
                        }
                    }
                    else
                    {
                        p.modified = 1;
                    }
                }
            }
        }
    }
}

// --------
int main(int argc, char *argv[])
{
    cin.sync_with_stdio(0);
    cout.sync_with_stdio(0);
    string rfile = "", ifile = "";
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == 'f')
            {
                string temp = "";
                for (int j = 2; j < strlen(argv[i]); j++)
                {
                    temp += argv[i][j];
                }
                max_frames = stoi(temp);
                for (int j = 0; j < max_frames; j++)
                {
                    vf.push_back(frame_t(j));
                    freepool.push_back(j);
                }
            }
            else if (argv[i][1] == 'a')
            {
                if (argv[i][2] == 'F' || argv[i][2] == 'f')
                {
                    main_pager = new Fifo();
                }
                else if (argv[i][2] == 'R' || argv[i][2] == 'r')
                {
                    main_pager = new Random();
                }
                else if (argv[i][2] == 'C' || argv[i][2] == 'c')
                {
                    main_pager = new Clock();
                }
                else if (argv[i][2] == 'E' || argv[i][2] == 'e')
                {
                    main_pager = new NRU();
                }
                else if (argv[i][2] == 'A' || argv[i][2] == 'a')
                {
                    main_pager = new Aging();
                }
                else
                {
                    main_pager = new WS();
                }
            }
            else
            {
                for (int j = 1; j < strlen(argv[i]); j++)
                {
                    if (argv[i][j] == 'O')
                    {
                        option_O = true;
                    }
                    if (argv[i][j] == 'P')
                    {
                        option_P = true;
                    }
                    if (argv[i][j] == 'F')
                    {
                        option_F = true;
                    }
                    if (argv[i][j] == 'S')
                    {
                        option_S = true;
                    }
                }
            }
        }
        else if (ifile == "")
        {
            for (int j = 0; j < strlen(argv[i]); j++)
            {
                ifile += argv[i][j];
            }
            // cout << ifile << endl;
        }
        else
        {
            for (int j = 0; j < strlen(argv[i]); j++)
            {
                rfile += argv[i][j];
            }
            // cout << rfile << endl;
            readRandomNumbers(rfile);
        }
    }
    readInput(ifile);
    if (option_P)
    {
        for (int i = 0; i < vp.size(); i++)
        {
            cout << "PT[" << i << "]:";
            for (int j = 0; j < vp[i].vpte.size(); j++)
            {
                pte p = vp[i].vpte[j];
                if (!p.present)
                {
                    if (!p.pagedout)
                    {
                        cout << " *";
                    }
                    else
                    {
                        cout << " #";
                    }
                }
                else if (p.present)
                {
                    cout << " " << j << ":" << (p.referenced ? "R" : "-") << (p.modified ? "M" : "-") << (p.pagedout ? "S" : "-");
                }
            }
            cout << "\n";
        }
    }
    if (option_F)
    {
        cout << "FT:";
        for (int i = 0; i < max_frames; i++)
        {
            frame_t f = vf[i];
            if (f.allocated)
            {
                cout << " " << f.pid << ":" << f.vpage;
            }
            else
            {
                cout << " *";
            }
        }
        cout << "\n";
    }
    if (option_S)
    {
        for (int i = 0; i < vp.size(); i++)
        {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   vp[i].pid,
                   vp[i].unmaps, vp[i].maps, vp[i].ins, vp[i].outs,
                   vp[i].fins, vp[i].fouts, vp[i].zeros,
                   vp[i].segv, vp[i].segprot);
        }
        printf("TOTALCOST %lu %lu %lu %llu %lu\n",
               inst_count, ctx_switches, process_exits, cost, sizeof(pte));
    }
}