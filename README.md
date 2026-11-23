# Emergency Response & Ambulance Dispatch Simulator

A Windows desktop program written in C++ that simulates how an emergency service assigns ambulances to incoming cases. It was built as a Data Structures project.

## Features
- Add emergency cases with a type, location and severity
- Serves the most urgent case first
- Finds the nearest available ambulance using Dijkstra's algorithm
- Road network map: click a location to preview a route
- Colour-coded tables and map, with a history of dispatches and completions

## Data Structures Used
| Data structure | Used for |
|---|---|
| Weighted graph (adjacency list) | City locations and road distances |
| Dijkstra's algorithm | Shortest route to the nearest ambulance |
| Priority queue (binary min-heap) | Orders cases by severity |
| Queue (linked nodes) | Holds incoming requests |
| Linked list | Dispatch and completion history |
| Arrays (vectors) | Stores ambulances and cases |

## How to Run
1. Download the project files.
2. Double-click `AmbulanceDispatch.exe` (Windows only).

## How to Build
- **Dev-C++:** open `main.cpp`, then in Project Options > Parameters > Linker add `-mwindows -lcomctl32 -lgdi32`, and rebuild.
- **MinGW:** run `build.bat`.

## How to Use
1. Pick an emergency type, location and severity, then click **Add Case**.
2. Click **Dispatch Next Case** or **Dispatch All**.
3. Select a case in the Active Cases table and click **Complete Selected Case** to free its ambulance.

## Project Files
- `main.cpp` - GUI (Win32 API)
- `dispatch_system.h` - data structures and dispatch logic
- `build.bat`, `resource.rc`, `app.manifest` - build files

## Screenshot
(Add a screenshot of the program here)

## Built With
C++, native Win32 API (no external libraries)
