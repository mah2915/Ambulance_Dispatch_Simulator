// ============================================================
//  Emergency Response & Ambulance Dispatch Simulator
//  dispatch_system.h  -  all data structures and dispatch logic
//  (no GUI code here; the GUI lives in main.cpp)
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <climits>

const int INF = INT_MAX;

// ------------------------------------------------------------
//  Basic records (structures)
// ------------------------------------------------------------
enum Severity   { CRITICAL = 1, HIGH = 2, MEDIUM = 3, LOW = 4 };
enum CaseStatus { IN_QUEUE, PRIORITIZED, DISPATCHED, COMPLETED };

struct EmergencyCase {
    int         id;
    std::string type;
    int         location;      // vertex index in the road graph
    int         severity;      // 1 = Critical ... 4 = Low
    CaseStatus  status;
    int         ambulanceId;   // 0 = none yet
    int         distance;      // km travelled by the ambulance
};

struct Ambulance {
    int  id;
    int  location;             // vertex index
    bool available;
    int  caseId;               // 0 = none
};

// ------------------------------------------------------------
//  1) Weighted graph (adjacency list) + Dijkstra's algorithm
// ------------------------------------------------------------
struct Edge { int to; int weight; };

class Graph {
    std::vector<std::string>        names;
    std::vector<std::vector<Edge> > adj;
public:
    int addLocation(const std::string& name) {
        names.push_back(name);
        adj.push_back(std::vector<Edge>());
        return (int)names.size() - 1;
    }
    void addRoad(int a, int b, int km) {          // two-way road
        Edge e1 = { b, km };
        Edge e2 = { a, km };
        adj[a].push_back(e1);
        adj[b].push_back(e2);
    }
    int size() const { return (int)names.size(); }
    const std::string& name(int v) const { return names[v]; }
    const std::vector<Edge>& neighbors(int v) const { return adj[v]; }

    // Dijkstra: shortest distance from src to every vertex.
    // parent[v] is the previous vertex on the path (parent[src] = -1).
    void dijkstra(int src, std::vector<int>& dist, std::vector<int>& parent) const {
        int n = size();
        dist.assign(n, INF);
        parent.assign(n, -1);
        std::vector<bool> done(n, false);
        dist[src] = 0;
        for (int step = 0; step < n; step++) {
            int u = -1;
            for (int i = 0; i < n; i++)
                if (!done[i] && dist[i] != INF && (u == -1 || dist[i] < dist[u]))
                    u = i;
            if (u == -1) break;
            done[u] = true;
            for (size_t k = 0; k < adj[u].size(); k++) {
                int v = adj[u][k].to;
                int w = adj[u][k].weight;
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    parent[v] = u;
                }
            }
        }
    }
};

// ------------------------------------------------------------
//  2) Priority queue (binary min-heap) for emergency cases
//     Lower severity number = more urgent. Ties: older case first.
// ------------------------------------------------------------
struct HeapItem { int severity; int caseId; };

class PriorityQueue {
    std::vector<HeapItem> a;

    static bool before(const HeapItem& x, const HeapItem& y) {
        if (x.severity != y.severity) return x.severity < y.severity;
        return x.caseId < y.caseId;
    }
    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (!before(a[i], a[p])) break;
            HeapItem t = a[i]; a[i] = a[p]; a[p] = t;
            i = p;
        }
    }
    void siftDown(int i) {
        int n = (int)a.size();
        while (true) {
            int l = 2 * i + 1, r = 2 * i + 2, best = i;
            if (l < n && before(a[l], a[best])) best = l;
            if (r < n && before(a[r], a[best])) best = r;
            if (best == i) break;
            HeapItem t = a[i]; a[i] = a[best]; a[best] = t;
            i = best;
        }
    }
public:
    bool empty() const { return a.empty(); }
    int  size()  const { return (int)a.size(); }
    void push(const HeapItem& x) { a.push_back(x); siftUp((int)a.size() - 1); }
    HeapItem pop() {
        HeapItem top = a[0];
        a[0] = a.back();
        a.pop_back();
        if (!a.empty()) siftDown(0);
        return top;
    }
    // case ids in priority order (works on a copy, heap is unchanged)
    std::vector<int> sortedIds() const {
        PriorityQueue copy = *this;
        std::vector<int> ids;
        while (!copy.empty()) ids.push_back(copy.pop().caseId);
        return ids;
    }
};

// ------------------------------------------------------------
//  3) Queue (linked FIFO) - holds incoming requests temporarily
// ------------------------------------------------------------
class Queue {
    struct Node { int value; Node* next; };
    Node* head;
    Node* tail;
    Queue(const Queue&);              // not copyable
    Queue& operator=(const Queue&);
public:
    Queue() : head(0), tail(0) {}
    ~Queue() { while (!empty()) dequeue(); }
    bool empty() const { return head == 0; }
    void enqueue(int v) {
        Node* n = new Node; n->value = v; n->next = 0;
        if (tail) tail->next = n; else head = n;
        tail = n;
    }
    int dequeue() {
        Node* n = head;
        int v = n->value;
        head = n->next;
        if (!head) tail = 0;
        delete n;
        return v;
    }
    std::vector<int> toVector() const {
        std::vector<int> v;
        for (Node* p = head; p; p = p->next) v.push_back(p->value);
        return v;
    }
};

// ------------------------------------------------------------
//  4) Linked list - dispatch and completion history
// ------------------------------------------------------------
struct HistoryRecord {
    int         no;
    std::string event;       // "Dispatched" or "Completed"
    int         caseId;
    int         ambulanceId;
    int         distance;    // km (0 for completed)
};

class HistoryList {
    struct Node { HistoryRecord data; Node* next; };
    Node* head;
    Node* tail;
    int   count;
    HistoryList(const HistoryList&);
    HistoryList& operator=(const HistoryList&);
public:
    HistoryList() : head(0), tail(0), count(0) {}
    ~HistoryList() {
        while (head) { Node* n = head; head = head->next; delete n; }
    }
    void add(const std::string& event, int caseId, int ambId, int dist) {
        Node* n = new Node;
        n->data.no = ++count;
        n->data.event = event;
        n->data.caseId = caseId;
        n->data.ambulanceId = ambId;
        n->data.distance = dist;
        n->next = 0;
        if (tail) tail->next = n; else head = n;
        tail = n;
    }
    std::vector<HistoryRecord> toVector() const {
        std::vector<HistoryRecord> v;
        for (Node* p = head; p; p = p->next) v.push_back(p->data);
        return v;
    }
};

// ------------------------------------------------------------
//  The dispatch system - ties everything together
// ------------------------------------------------------------
class DispatchSystem {
public:
    Graph                      city;
    std::vector<EmergencyCase> cases;       // array of all cases (id = index + 1)
    std::vector<Ambulance>     ambulances;  // array of ambulances (id = index + 1)
    Queue                      incoming;    // new requests wait here
    PriorityQueue              pending;     // prioritized cases
    HistoryList                history;     // linked list of events
    std::vector<int>           lastRoute;   // route of the latest dispatch (for the map)

    DispatchSystem() { buildCity(); addAmbulance(0); addAmbulance(7); addAmbulance(3); }

    static std::string severityName(int s) {
        switch (s) {
            case CRITICAL: return "Critical";
            case HIGH:     return "High";
            case MEDIUM:   return "Medium";
            default:       return "Low";
        }
    }
    static std::string statusName(CaseStatus s) {
        switch (s) {
            case IN_QUEUE:    return "In Queue";
            case PRIORITIZED: return "Prioritized";
            case DISPATCHED:  return "Dispatched";
            default:          return "Completed";
        }
    }

    // ---- adding data ----
    int addCase(const std::string& type, int location, int severity) {
        EmergencyCase c;
        c.id = (int)cases.size() + 1;
        c.type = type;
        c.location = location;
        c.severity = severity;
        c.status = IN_QUEUE;
        c.ambulanceId = 0;
        c.distance = 0;
        cases.push_back(c);
        incoming.enqueue(c.id);
        return c.id;
    }
    int addAmbulance(int location) {
        Ambulance a;
        a.id = (int)ambulances.size() + 1;
        a.location = location;
        a.available = true;
        a.caseId = 0;
        ambulances.push_back(a);
        return a.id;
    }

    // ---- queue -> priority queue ----
    void moveQueueToHeap() {
        while (!incoming.empty()) {
            int id = incoming.dequeue();
            EmergencyCase& c = cases[id - 1];
            c.status = PRIORITIZED;
            HeapItem h = { c.severity, c.id };
            pending.push(h);
        }
    }

    int availableAmbulances() const {
        int n = 0;
        for (size_t i = 0; i < ambulances.size(); i++)
            if (ambulances[i].available) n++;
        return n;
    }

    // ---- dispatch the most urgent case ----
    // returns true if a case was dispatched; msg describes what happened
    bool dispatchNext(std::string& msg) {
        moveQueueToHeap();
        if (pending.empty()) {
            msg = "No pending emergency cases.";
            return false;
        }
        if (availableAmbulances() == 0) {
            msg = "No ambulance is available. Pending cases will wait until one is free.";
            return false;
        }

        HeapItem top = pending.pop();
        EmergencyCase& c = cases[top.caseId - 1];

        // Dijkstra from the emergency location (roads are two-way, so the
        // distance is the same in both directions)
        std::vector<int> dist, parent;
        city.dijkstra(c.location, dist, parent);

        int best = nearestAvailable(dist);
        if (best == -1) {                       // no ambulance can reach it
            pending.push(top);
            msg = "No available ambulance can reach case #" + num(c.id) + ".";
            return false;
        }

        Ambulance& a = ambulances[best];
        int km = dist[a.location];

        // route: ambulance location -> ... -> emergency location
        lastRoute = pathFrom(a.location, parent);
        std::string route = routeText(lastRoute);
        std::string from = city.name(a.location);

        // update ambulance and case
        a.available = false;
        a.caseId = c.id;
        a.location = c.location;
        c.status = DISPATCHED;
        c.ambulanceId = a.id;
        c.distance = km;
        history.add("Dispatched", c.id, a.id, km);

        msg = "Case #" + num(c.id) + "  [" + severityName(c.severity) + "]  " + c.type +
              " at " + city.name(c.location) + "\n" +
              "  Ambulance #" + num(a.id) + " dispatched from " + from + "\n" +
              "  Distance : " + num(km) + " km\n" +
              "  Route    : " + route;
        return true;
    }

    // ---- dispatch until no cases or no ambulances are left ----
    int dispatchAll(std::string& msg) {
        int count = 0;
        std::string text, one;
        while (dispatchNext(one)) {
            if (!text.empty()) text += "\n";
            text += one;
            count++;
        }
        if (count == 0) { msg = one; return 0; }
        if (!pending.empty()) text += "\n" + one;   // why it stopped
        msg = text;
        return count;
    }

    // ---- mark a case as completed, ambulance becomes available ----
    bool completeCase(int caseId, std::string& msg) {
        if (caseId < 1 || caseId > (int)cases.size() ||
            cases[caseId - 1].status != DISPATCHED) {
            msg = "That case is not active.";
            return false;
        }
        EmergencyCase& c = cases[caseId - 1];
        Ambulance& a = ambulances[c.ambulanceId - 1];
        c.status = COMPLETED;
        a.available = true;
        a.caseId = 0;
        history.add("Completed", c.id, a.id, 0);
        msg = "Case #" + num(c.id) + " completed. Ambulance #" + num(a.id) +
              " is available again at " + city.name(a.location) + ".";
        return true;
    }

    // ---- map click: which available ambulance is nearest to a location? ----
    bool previewRoute(int location, int& ambId, int& km, std::vector<int>& route) {
        std::vector<int> dist, parent;
        city.dijkstra(location, dist, parent);
        int best = nearestAvailable(dist);
        if (best == -1) return false;
        ambId = ambulances[best].id;
        km = dist[ambulances[best].location];
        route = pathFrom(ambulances[best].location, parent);
        return true;
    }

    std::string routeText(const std::vector<int>& route) const {
        std::string t;
        for (size_t i = 0; i < route.size(); i++) {
            if (i) t += " -> ";
            t += city.name(route[i]);
        }
        return t;
    }

    static std::string num(int n) {
        char buf[32];
        int i = 0, neg = n < 0;
        unsigned u = neg ? (unsigned)(-n) : (unsigned)n;
        do { buf[i++] = (char)('0' + u % 10); u /= 10; } while (u);
        if (neg) buf[i++] = '-';
        std::string s;
        while (i) s += buf[--i];
        return s;
    }

private:
    // index of the available ambulance with the smallest distance (-1 if none)
    int nearestAvailable(const std::vector<int>& dist) const {
        int best = -1;
        for (size_t i = 0; i < ambulances.size(); i++) {
            if (!ambulances[i].available) continue;
            int d = dist[ambulances[i].location];
            if (d == INF) continue;
            if (best == -1 || d < dist[ambulances[best].location]) best = (int)i;
        }
        return best;
    }
    // follow Dijkstra parents: start vertex -> ... -> source
    static std::vector<int> pathFrom(int start, const std::vector<int>& parent) {
        std::vector<int> path;
        for (int v = start; v != -1; v = parent[v]) path.push_back(v);
        return path;
    }

    // Fixed city road network (distances in km)
    void buildCity() {
        const char* places[] = {
            "Saddar", "Clifton", "DHA", "Korangi", "Landhi",
            "Malir", "Airport", "Gulshan-e-Iqbal", "North Nazimabad", "Orangi Town"
        };
        for (int i = 0; i < 10; i++) city.addLocation(places[i]);

        city.addRoad(0, 1, 6);    // Saddar - Clifton
        city.addRoad(0, 2, 10);   // Saddar - DHA
        city.addRoad(0, 7, 11);   // Saddar - Gulshan
        city.addRoad(1, 2, 5);    // Clifton - DHA
        city.addRoad(2, 3, 9);    // DHA - Korangi
        city.addRoad(3, 4, 6);    // Korangi - Landhi
        city.addRoad(3, 6, 10);   // Korangi - Airport
        city.addRoad(4, 5, 7);    // Landhi - Malir
        city.addRoad(5, 6, 8);    // Malir - Airport
        city.addRoad(6, 7, 7);    // Airport - Gulshan
        city.addRoad(7, 8, 6);    // Gulshan - North Nazimabad
        city.addRoad(8, 9, 8);    // North Nazimabad - Orangi
        city.addRoad(0, 8, 14);   // Saddar - North Nazimabad
    }
};
