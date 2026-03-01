/*
 * ============================================================
 *   SIMULADOR DE ALGORITMOS DE REEMPLAZO DE PAGINAS
 *   FIFO | Optimo | LRU | LFU | MFU | Reloj | NRU | Segunda Oportunidad
 * ============================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <climits>
#include <sstream>

using namespace std;
using namespace chrono;

#define RST  "\033[0m"
#define BOLD "\033[1m"
#define RED  "\033[31m"
#define GRN  "\033[32m"
#define YEL  "\033[33m"
#define CYN  "\033[36m"
#define MAG  "\033[35m"
#define DIM  "\033[2m"

struct Step {
    int t, ref, victim;
    bool hit;
    vector<int> frames;
    string note;
};

struct Metrics {
    int hits = 0, faults = 0, sys_calls = 0, interrupts = 0, total = 0;
    double time_us = 0.0;
    long mem_bytes = 0;
};

void sep(char c = '-', int w = 72) { cout << string(w, c) << "\n"; }

void banner(const string& t) {
    sep('='); cout << BOLD << CYN << "  " << t << RST << "\n"; sep('=');
}

// ─────────────────────────────────────────────
//  TABLA HORIZONTAL
// ─────────────────────────────────────────────
void printTableH(const vector<Step>& steps, int N, const string& algo) {
    int total = (int)steps.size();
    const int BLOCK = 18;

    cout << BOLD << CYN << "\n  TABLA DE REEMPLAZO — " << algo << RST << "\n";

    for (int s = 0; s < total; s += BLOCK) {
        int e = min(s + BLOCK, total);
        sep();

        cout << BOLD << left << setw(14) << "  Tiempo" << RST;
        for (int i = s; i < e; i++) cout << setw(5) << ("T" + to_string(steps[i].t));
        cout << "\n";

        cout << BOLD << left << setw(14) << "  Ref" << RST;
        for (int i = s; i < e; i++) {
            if (steps[i].hit) cout << GRN << BOLD << setw(5) << steps[i].ref << RST;
            else             cout << RED << BOLD << setw(5) << steps[i].ref << RST;
        }
        cout << "\n";
        sep('-');

        for (int m = 0; m < N; m++) {
            cout << BOLD << left << setw(14) << ("  Marco " + to_string(m + 1)) << RST;
            for (int i = s; i < e; i++) {
                int val = (m < (int)steps[i].frames.size()) ? steps[i].frames[m] : -1;
                string cell = (val == -1) ? "-" : to_string(val);
                bool newLoad = !steps[i].hit && val == steps[i].ref
                    && (i == 0 || steps[i - 1].frames.size() <= (size_t)m || steps[i - 1].frames[m] != val);
                bool firstFill = !steps[i].hit && steps[i].victim == -1 && val == steps[i].ref
                    && (i == 0 || steps[i - 1].frames[m] == -1);
                bool hitPage = steps[i].hit && steps[i].ref == val;
                if (hitPage)                   cout << GRN << BOLD << setw(5) << cell << RST;
                else if (newLoad || firstFill) cout << YEL << BOLD << setw(5) << cell << RST;
                else                           cout << setw(5) << cell;
            }
            cout << "\n";
        }
        sep('-');

        cout << BOLD << left << setw(14) << "  Victima" << RST;
        for (int i = s; i < e; i++) {
            if (steps[i].victim != -1) cout << RED << setw(5) << steps[i].victim << RST;
            else                       cout << setw(5) << "-";
        }
        cout << "\n";

        cout << BOLD << left << setw(14) << "  Resultado" << RST;
        for (int i = s; i < e; i++) {
            if (steps[i].hit) cout << GRN << BOLD << setw(5) << "A" << RST;
            else             cout << RED << BOLD << setw(5) << "F" << RST;
        }
        cout << "\n";

        bool hasNote = false;
        for (int i = s; i < e; i++) if (!steps[i].note.empty()) { hasNote = true; break; }
        if (hasNote) {
            cout << BOLD << left << setw(14) << "  Nota" << RST;
            for (int i = s; i < e; i++) {
                string n = steps[i].note.empty() ? "-" : steps[i].note;
                if (n.size() > 4) n = n.substr(0, 4);
                cout << setw(5) << n;
            }
            cout << "\n";
        }
        if (e < total) { sep('.'); cout << DIM << "  (continua...)\n" << RST; }
    }
    sep();
}

void printBarChart(const vector<Step>& steps, const string& algo) {
    int h = 0, f = 0;
    for (auto& s : steps) s.hit ? h++ : f++;
    int total = (int)steps.size(), W = 36;
    int hB = total ? (int)((double)h / total * W) : 0;
    int fB = total ? (int)((double)f / total * W) : 0;

    cout << BOLD << "\n  GRAFICA — " << algo << RST << "\n";
    sep();
    cout << "  Aciertos  " << GRN << string(hB, '#') << RST << string(W - hB, '.') << "  " << h << "/" << total << "\n";
    cout << "  Fallos    " << RED << string(fB, '#') << RST << string(W - fB, '.') << "  " << f << "/" << total << "\n";
    cout << "\n  Timeline: ";
    for (auto& s : steps) cout << (s.hit ? GRN : RED) << (s.hit ? "A" : "F") << RST;
    cout << "\n";
    sep();
}

void printMetrics(const string& algo, const Metrics& m) {
    sep('=');
    cout << BOLD << MAG << "  METRICAS — " << algo << RST << "\n";
    sep();
    double hr = m.total ? (double)m.hits / m.total * 100 : 0;
    double fr = m.total ? (double)m.faults / m.total * 100 : 0;

    cout << BOLD << "  1. Recursos utilizados\n" << RST;
    cout << "     Marcos en memoria    : " << CYN << m.mem_bytes << " bytes" << RST << "\n";
    cout << "     Referencias totales  : " << m.total << "\n";
    cout << BOLD << "\n  2. Tiempo de ejecucion\n" << RST;
    cout << "     " << YEL << fixed << setprecision(4) << m.time_us << " microsegundos" << RST << "\n";
    cout << BOLD << "\n  3. Llamadas al sistema\n" << RST;
    cout << "     sys_calls            : " << CYN << m.sys_calls << RST << "  (una por cada pagina cargada)\n";
    cout << BOLD << "\n  4. Interrupciones\n" << RST;
    cout << "     Page Faults (IRQ)    : " << RED << m.interrupts << RST << "  (una por cada fallo de pagina)\n";
    cout << BOLD << "\n  5. Resumen\n" << RST;
    cout << "     Aciertos             : " << GRN << m.hits << RST << "\n";
    cout << "     Fallos               : " << RED << m.faults << RST << "\n";
    cout << "     Tasa de acierto      : " << GRN << fixed << setprecision(1) << hr << "%" << RST << "\n";
    cout << "     Tasa de fallo        : " << RED << fixed << setprecision(1) << fr << "%" << RST << "\n";
    sep('=');
}

// ─────────────────────────────────────────────
//  ENTRADA DE DATOS
// ─────────────────────────────────────────────
vector<int> pedirRefs() {
    cout << CYN << "\n  Referencias de paginas (separadas por espacio):\n  > " << RST;
    string line; getline(cin, line);
    vector<int> refs; istringstream iss(line); int x;
    while (iss >> x) refs.push_back(x);
    return refs;
}

int pedirMarcos() {
    int n = 0;
    while (n <= 0) {
        cout << CYN << "  Numero de marcos: " << RST;
        cin >> n; cin.ignore();
        if (n <= 0) cout << RED << "  Debe ser mayor a 0.\n" << RST;
    }
    return n;
}

vector<char> pedirOps(int n) {
    cout << CYN << "\n  Operacion por cada referencia (R=lectura, W=escritura).\n";
    cout << "  Ingresa " << n << " valores, ej: R W R W R ...\n  > " << RST;
    string line; getline(cin, line);
    vector<char> ops; istringstream iss(line); char c;
    while (iss >> c) ops.push_back(toupper(c));
    while ((int)ops.size() < n) ops.push_back('R');
    return ops;
}

// ─────────────────────────────────────────────
//  ALGORITMOS
// ─────────────────────────────────────────────
vector<Step> algo_FIFO(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), order(N, INT_MAX);
    int oc = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; order[idx] = oc++; }
            else {
                int vs = min_element(order.begin(), order.end()) - order.begin();
                s.victim = frames[vs]; frames[vs] = ref; order[vs] = oc++;
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_Optimal(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1);
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { frames[emp - frames.begin()] = ref; }
            else {
                vector<int> fut(refs.begin() + i + 1, refs.end());
                int vs = 0, maxD = -1;
                for (int j = 0; j < N; j++) {
                    auto fi = find(fut.begin(), fut.end(), frames[j]);
                    int d = (fi == fut.end()) ? INT_MAX : (int)(fi - fut.begin());
                    if (d > maxD) { maxD = d; vs = j; }
                }
                s.victim = frames[vs]; frames[vs] = ref;
                if (maxD == INT_MAX) s.note = "INF";
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_LRU(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), lu(N, -1);
    int t = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i]; t++;
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; lu[it - frames.begin()] = t; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; lu[idx] = t; }
            else { int vs = min_element(lu.begin(), lu.end()) - lu.begin(); s.victim = frames[vs]; frames[vs] = ref; lu[vs] = t; }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_LFU(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), freq(N, 0), lo(N, INT_MAX);
    int lc = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; freq[it - frames.begin()]++; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; freq[idx] = 1; lo[idx] = lc++; }
            else {
                int vs = 0;
                for (int j = 1; j < N; j++) if (freq[j] < freq[vs] || (freq[j] == freq[vs] && lo[j] < lo[vs]))vs = j;
                s.victim = frames[vs]; frames[vs] = ref; freq[vs] = 1; lo[vs] = lc++;
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_MFU(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), freq(N, 0), lo(N, INT_MAX);
    unordered_map<int, int> gfreq;
    int lc = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i]; gfreq[ref]++;
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; freq[it - frames.begin()] = gfreq[ref]; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; freq[idx] = gfreq[ref]; lo[idx] = lc++; }
            else {
                int vs = 0;
                for (int j = 1; j < N; j++) if (freq[j] > freq[vs] || (freq[j] == freq[vs] && lo[j] < lo[vs]))vs = j;
                s.victim = frames[vs]; frames[vs] = ref; freq[vs] = gfreq[ref]; lo[vs] = lc++;
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_Reloj(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), bits(N, 0);
    int ptr = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; bits[it - frames.begin()] = 1; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; bits[idx] = 1; ptr = (idx + 1) % N; }
            else {
                bool allOne = true;
                for (int j = 0; j < N; j++) if (bits[j] == 0) { allOne = false; break; }
                if (allOne) { for (int j = 0; j < N; j++)bits[j] = 0; s.note = "RST"; }
                while (bits[ptr] == 1) { bits[ptr] = 0; ptr = (ptr + 1) % N; }
                s.victim = frames[ptr]; frames[ptr] = ref; bits[ptr] = 1; ptr = (ptr + 1) % N;
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_NRU(const vector<int>& refs, int N, Metrics& m,
    const vector<char>& ops, int reset_every = 4) {
    vector<Step> steps;
    vector<int> frames(N, -1), bitR(N, 0), bitM(N, 0);
    int t = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        char op = (i < (int)ops.size()) ? ops[i] : 'R';
        t++;
        bool rst = (t > 1 && (t - 1) % reset_every == 0);
        if (rst) for (int j = 0; j < N; j++)bitR[j] = 0;
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        if (rst)s.note = "RST ";
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; int idx = it - frames.begin(); bitR[idx] = 1; bitM[idx] = (op == 'W') ? 1 : 0; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; bitR[idx] = 1; bitM[idx] = (op == 'W') ? 1 : 0; }
            else {
                int minC = 4, vs = 0;
                for (int j = 0; j < N; j++) { int c = bitR[j] * 2 + bitM[j]; if (c < minC) { minC = c; vs = j; } }
                s.victim = frames[vs]; frames[vs] = ref; bitR[vs] = 1; bitM[vs] = (op == 'W') ? 1 : 0;
                s.note += "C" + to_string(minC);
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

vector<Step> algo_SecondChance(const vector<int>& refs, int N, Metrics& m) {
    vector<Step> steps;
    vector<int> frames(N, -1), bits(N, 0);
    int ptr = 0;
    for (int i = 0; i < (int)refs.size(); i++) {
        int ref = refs[i];
        Step s; s.t = i + 1; s.ref = ref; s.victim = -1;
        auto it = find(frames.begin(), frames.end(), ref);
        s.hit = it != frames.end();
        if (s.hit) { m.hits++; bits[it - frames.begin()] = 1; }
        else {
            m.faults++; m.interrupts++; m.sys_calls++;
            auto emp = find(frames.begin(), frames.end(), -1);
            if (emp != frames.end()) { int idx = emp - frames.begin(); frames[idx] = ref; bits[idx] = 1; ptr = idx; }
            else {
                int search = (ptr + 1) % N;
                while (true) {
                    if (bits[search] == 0) { s.victim = frames[search]; frames[search] = ref; bits[search] = 1; ptr = search; break; }
                    else { bits[search] = 0; search = (search + 1) % N; }
                }
            }
        }
        s.frames = frames; steps.push_back(s);
    }
    return steps;
}

// ─────────────────────────────────────────────
//  EJECUTAR UN ALGORITMO
//  Orden del menu: 1=FIFO 2=OPT 3=LRU 4=LFU 5=MFU 6=RELOJ 7=NRU 8=SEG.OPORT
// ─────────────────────────────────────────────
void runOne(int menuOp) {
    struct AlgoInfo { string name; int id; };
    AlgoInfo table[] = {
        {},
        {"FIFO",               1},
        {"OPTIMO",             2},
        {"LRU",                3},
        {"LFU",                4},
        {"MFU",                5},
        {"RELOJ",              6},
        {"NRU",                7},
        {"SEGUNDA OPORTUNIDAD",8}
    };
    AlgoInfo& ai = table[menuOp];
    banner("ALGORITMO: " + ai.name);

    vector<int> refs = pedirRefs();
    if (refs.empty()) { cout << RED << "  Secuencia vacia. Cancelado.\n" << RST; return; }
    int N = pedirMarcos();
    vector<char> ops;
    if (ai.id == 7) ops = pedirOps((int)refs.size());
    else            ops.assign(refs.size(), 'R');

    Metrics m;
    m.total = (int)refs.size();
    m.mem_bytes = (long)N * sizeof(int) + (long)refs.size() * sizeof(int);

    vector<Step> steps;
    auto t0 = high_resolution_clock::now();
    switch (ai.id) {
    case 1: steps = algo_FIFO(refs, N, m);         break;
    case 2: steps = algo_Optimal(refs, N, m);      break;
    case 3: steps = algo_LRU(refs, N, m);          break;
    case 4: steps = algo_LFU(refs, N, m);          break;
    case 5: steps = algo_MFU(refs, N, m);          break;
    case 6: steps = algo_Reloj(refs, N, m);        break;
    case 7: steps = algo_NRU(refs, N, m, ops);     break;
    case 8: steps = algo_SecondChance(refs, N, m); break;
    }
    auto t1 = high_resolution_clock::now();
    m.time_us = duration_cast<nanoseconds>(t1 - t0).count() / 1000.0;

    printTableH(steps, N, ai.name);
    printBarChart(steps, ai.name);
    printMetrics(ai.name, m);
}

// ─────────────────────────────────────────────
//  MENU
// ─────────────────────────────────────────────
void showMenu() {
    cout << "\n";
    sep('=');
    cout << BOLD << CYN << "      SIMULADOR DE REEMPLAZO DE PAGINAS\n" << RST;
    sep('=');
    cout << BOLD;
    cout << "   [1]  FIFO                ->  First In, First Out\n";
    cout << "   [2]  OPTIMO              ->  Optimo / Belady\n";
    cout << "   [3]  LRU                 ->  Least Recently Used\n";
    cout << "   [4]  LFU                 ->  Least Frequently Used\n";
    cout << "   [5]  MFU                 ->  Most Frequently Used\n";
    cout << "   [6]  RELOJ               ->  Clock Algorithm\n";
    cout << "   [7]  NRU                 ->  Not Recently Used\n";
    cout << "   [8]  SEGUNDA OPORTUNIDAD ->  Second Chance\n";
    cout << RST;
    sep('-');
    cout << BOLD << "   [0]  " << RED << "SALIR\n" << RST;
    sep('=');
    cout << BOLD << "   Opcion: " << RST;
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main() {
    cout << BOLD << GRN;
    sep('=');
    cout << "   SIMULADOR DE ALGORITMOS DE REEMPLAZO DE PAGINAS\n";
    cout << "   FIFO | OPTIMO | LRU | LFU | MFU | RELOJ | NRU | SEGUNDA OPORTUNIDAD\n";
    sep('=');
    cout << RST;

    int op = -1;
    while (op != 0) {
        showMenu(); cin >> op; cin.ignore();
        if (op >= 1 && op <= 8) {
            runOne(op);
        }
        else if (op != 0) {
            cout << RED << "  Opcion invalida.\n" << RST;
        }
    }
    cout << BOLD << GRN << "\n  Hasta luego!\n" << RST;
    return 0;
}