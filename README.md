# 3D Dynamic Emergency Hospital Management and Resource Allocation Simulator

An interactive 3D Hospital Emergency Management and Resource Allocation simulation built using **C++** and **OpenGL (FreeGLUT / GLU)**.

## 📌 Project Overview
This project simulates real-world emergency department (ED) operations, patient flow, triage categorization, doctor interventions, ambulance arrivals, and dynamic bed/staff reallocation under surge conditions.

### ✨ Key Features
1. **Real-time Patient Pathfinding & Movement**: Smooth animated walking from entrance or ambulance bay along corridors, safely reaching allocated hospital beds.
2. **Doctor Interaction & Medical Care**: Doctors navigate to admitted patients, perform examination & treatment animations with live indicators, and return to duty stations.
3. **Emergency Ambulance Integration**: Ambulances drive into the emergency bay with animated sirens and flashing beacons, docking to deliver critical triage patients.
4. **Dynamic Resource Allocation**: Automated shifts of idle doctors and overflow beds to the critical care zones when surge capacity is breached.
5. **7 Clinical Triage Zones**:
   - 🔴 **Red Zone**: Critical Care
   - 🟡 **Yellow Zone**: Urgent Care
   - 🟢 **Green Zone**: Minor Injuries
   - 🔵 **Blue Zone**: Observation
   - 🟣 **Purple Zone**: Specialized Care
   - ⚪ **White Zone**: General Support
   - 🟠 **Orange Zone**: Surge / Overflow
6. **Live Hospital Dashboard**: Real-time HUD displaying zone statistics, bed occupancy, doctor availability, ambulance status, and emergency alerts.
7. **Collision Avoidance**: Corridor waypoint navigation preventing patient and doctor models from clipping through walls, desks, and hospital beds.
8. **Day & Night / Emergency Lighting**: Toggleable lighting modes with directional illumination and emergency warning sirens.

---

## 🎮 Interactive Controls

### Simulation Actions
| Key | Action |
|-----|--------|
| `P` | Admit Random Patient (Automatic Triage) |
| `1` - `7` | Admit Patient directly to Zones (1: Red, 2: Yellow, 3: Green, 4: Blue, 5: Purple, 6: White, 7: Orange) |
| `D` | Discharge Treated Patient |
| `A` | Dispatch Ambulance Emergency Run |
| `O` | Trigger Dynamic Surge Resource Allocation |
| `L` | Toggle Day / Night Emergency Lighting |
| `R` | Reset Simulation to Default State |
| `ESC` | Exit Simulator |

### Camera & Navigation
| Control | Action |
|---------|--------|
| `C` | Cycle Preset Camera Views (Isometric, Top-Down, Red Zone, etc.) |
| `Arrow Keys` | Rotate Camera (Yaw / Pitch) |
| `Page Up / Down` | Zoom In / Out |
| `Mouse Drag` | Orbit & Pan Camera View |

---

## 🛠️ Build and Setup

### Prerequisites
- **Compiler**: GCC / MinGW (with C++11 or later)
- **Libraries**:
  - `freeglut` / `glut`
  - `opengl32`
  - `glu32`
- **IDE**: [Code::Blocks](https://www.codeblocks.org/) (or any standard C++ IDE)

### Running via Code::Blocks
1. Open `FCG_FINAL/FCG_FINAL.cbp` in Code::Blocks.
2. Ensure MinGW has FreeGLUT linked (`-lfreeglut -lopengl32 -lglu32`).
3. Click **Build and Run** (`F9`).
