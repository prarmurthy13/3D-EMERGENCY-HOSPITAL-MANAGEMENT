/*
 ============================================================================
  Project Title: 3D Dynamic Crowd Emergency Management and Resource Allocation
                 Simulator Using OpenGL
  Course: Computer Graphics (FCG Mini-Project)
  Environment: C++ with OpenGL / GLU / FreeGLUT (Code::Blocks Compatible)
  
  Key Extensions:
  1. Patient Movement: Real-time pathfinding & smooth animated walking from
     entrance/ambulance along corridors, safely reaching beds & lying down.
  2. Doctor Interaction: Doctors navigate to admitted patients, perform medical
     treatment animation with examination motion & treatment indicator, then return.
  3. Ambulance Integration: Ambulance drives in, flashes beacons, docks at bay,
     and unloads incoming emergency patients who walk directly into the ED.
  4. Dynamic Resource Allocation: Auto-shifts idle doctors and overflow beds to
     overcrowded Red Critical Zone when surge capacity is reached.
  5. 7 Triage Zones: Red (Critical), Yellow (Urgent), Green (Minor),
     Blue (Observation), Purple (Specialized), White (General Support), Orange (Overflow).
  6. Live Dashboard: Real-time statistics for all 7 zones, occupied/available beds,
     doctor readiness, ambulance status & surge indicators.
  7. Collision Avoidance: Corridor waypoint system prevents characters from clipping
     through walls, beds, and reception desks.
 ============================================================================
*/

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// DATA STRUCTURES & CONSTANTS
// ============================================================================

enum ZoneType {
    ZONE_RED = 0,     // Critical (Immediate Life Support)
    ZONE_YELLOW = 1,  // Urgent (Serious but stable)
    ZONE_GREEN = 2,   // Minor (Walking wounded / non-urgent)
    ZONE_BLUE = 3,    // Observation (Monitoring & diagnostics)
    ZONE_PURPLE = 4,  // Specialized Treatment (Trauma & surgery prep)
    ZONE_WHITE = 5,   // General Support (Triage & recovery)
    ZONE_ORANGE = 6   // Overflow Surge (Emergency overflow ward)
};

const int NUM_ZONES = 7;

const char* ZONE_NAMES[NUM_ZONES] = {
    "RED (Critical)",
    "YELLOW (Urgent)",
    "GREEN (Minor)",
    "BLUE (Observation)",
    "PURPLE (Specialized)",
    "WHITE (Support)",
    "ORANGE (Overflow)"
};

enum CameraView {
    VIEW_ISOMETRIC = 0,
    VIEW_TOP_DOWN = 1,
    VIEW_RED_ZONE = 2,
    VIEW_AMBULANCE_BAY = 3,
    VIEW_TRIAGE_DESK = 4,
    VIEW_SPECIALIZED_WING = 5
};

enum DoctorState {
    DOC_IDLE_PATROL = 0,
    DOC_MOVING_TO_PATIENT = 1,
    DOC_TREATING = 2,
    DOC_RETURNING = 3
};

struct Waypoint {
    float x, z;
};

struct Bed {
    int id;
    float x, z;
    float rotation;
    ZoneType zone;
    bool occupied;
    int patientId;
    float conditionPulse;
    bool isReallocated; // Borrowed by Red Zone during surge
};

struct WalkingPatient {
    int id;
    ZoneType assignedZone;
    int targetBedIndex;
    float x, z;
    std::vector<Waypoint> path;
    int pathIndex;
    float speed;
    float rotation;
    float legAngle;
    bool legForward;
    bool arrived;
    float rColor, gColor, bColor;
};

struct Doctor {
    int id;
    float x, z;
    float homeX, homeZ;
    float targetX, targetZ;
    std::vector<Waypoint> path;
    int pathIndex;
    float speed;
    float rotation;
    float legAngle;
    bool legForward;
    ZoneType assignedZone;
    ZoneType homeZone;
    DoctorState state;
    int treatingBedIndex;
    float treatmentTimer;
    float treatAnimAngle;
    bool isReallocated;
};

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================

int windowWidth = 1200;
int windowHeight = 750;

CameraView currentView = VIEW_ISOMETRIC;
float camDist = 54.0f;
float camYaw = 45.0f;
float camPitch = 38.0f;
float camTargetX = 0.0f;
float camTargetY = 0.0f;
float camTargetZ = 0.0f;

bool isDragging = false;
int lastMouseX = 0;
int lastMouseY = 0;

bool nightMode = false;
int simTimeMs = 0;
float beaconTimer = 0.0f;
bool beaconState = false;
float heartBeatTimer = 0.0f;

// Ambulance State
float ambX = 14.0f;
float ambZ = 34.0f;
float ambTargetZ = 16.0f;
float ambSpeed = 0.22f;
bool ambActive = false;
bool ambArrived = false;
float ambWaitTimer = 0.0f;
bool ambSirenActive = false;
bool ambDischargedPatient = false;

// Hospital Entities
std::vector<Bed> beds;
std::vector<Doctor> doctors;
std::vector<WalkingPatient> walkingPatients;

// Statistics & Allocation Status
int nextPatientId = 101;
int totalAdmitted = 0;
int totalDischarged = 0;
bool surgeModeActive = false;
std::string statusMessage = "Simulator Ready. Press [P] or [1-7] to admit patients.";

GLUquadricObj* quadric = NULL;

// ============================================================================
// UTILITY FUNCTIONS: DRAWING PRIMITIVES & TEXT
// ============================================================================

void renderBitmapString(float x, float y, void *font, const char *string) {
    glRasterPos2f(x, y);
    while (*string) {
        glutBitmapCharacter(font, *string);
        string++;
    }
}

void drawSolidBox(float dx, float dy, float dz) {
    glPushMatrix();
    glTranslatef(0.0f, dy * 0.5f, 0.0f);
    glScalef(dx, dy, dz);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void drawCylinderY(float radius, float height, int slices = 16) {
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quadric, radius, radius, height, slices, 2);
    glPopMatrix();
}

void setMaterial(float r, float g, float b, float shininess = 32.0f, float spec = 0.35f) {
    GLfloat mat_ambient[] = { r * 0.4f, g * 0.4f, b * 0.4f, 1.0f };
    GLfloat mat_diffuse[] = { r, g, b, 1.0f };
    GLfloat mat_specular[] = { spec, spec, spec, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glColor3f(r, g, b);
}

// Get Zone Theme Color
void getZoneColor(ZoneType z, float &r, float &g, float &b) {
    switch (z) {
        case ZONE_RED:    r = 0.88f; g = 0.18f; b = 0.18f; break; // Red
        case ZONE_YELLOW: r = 0.90f; g = 0.78f; b = 0.12f; break; // Yellow
        case ZONE_GREEN:  r = 0.18f; g = 0.76f; b = 0.32f; break; // Green
        case ZONE_BLUE:   r = 0.18f; g = 0.46f; b = 0.88f; break; // Blue
        case ZONE_PURPLE: r = 0.65f; g = 0.18f; b = 0.82f; break; // Purple
        case ZONE_WHITE:  r = 0.85f; g = 0.88f; b = 0.92f; break; // White / Silver
        case ZONE_ORANGE: r = 0.95f; g = 0.48f; b = 0.10f; break; // Orange
    }
}

// ============================================================================
// COLLISION AVOIDANCE & WAYPOINT CORRIDOR NAVIGATION
// ============================================================================

// Build collision-free corridor path between start (sx, sz) and destination (tx, tz)
std::vector<Waypoint> buildCorridorPath(float sx, float sz, float tx, float tz) {
    std::vector<Waypoint> path;

    // Hospital main corridor hubs:
    // Central North-South Corridor: X = 0.0f
    // East-West Main Crossings: Z = 4.0f (central crossway), Z = -8.0f (north crossway)
    // Triage / Entrance Crossway: Z = 14.0f

    // 1. Move to nearest access lane from start
    float startCorridorX = (sx < -8.0f) ? -8.0f : ((sx > 8.0f) ? 8.0f : 0.0f);
    float targetCorridorX = (tx < -8.0f) ? -8.0f : ((tx > 8.0f) ? 8.0f : 0.0f);

    path.push_back({ sx, sz });

    // Step 1: Align with local aisle
    if (std::abs(sx - startCorridorX) > 0.4f) {
        path.push_back({ startCorridorX, sz });
    }

    // Step 2: Route through main Central Corridor if switching east/west wings
    if (startCorridorX != targetCorridorX) {
        // Move to central crossing (Z = 4.0f)
        path.push_back({ startCorridorX, 4.0f });
        path.push_back({ 0.0f, 4.0f });
        path.push_back({ targetCorridorX, 4.0f });
    }

    // Step 3: Traverse target corridor to target Z level
    path.push_back({ targetCorridorX, tz });

    // Step 4: Step into the bedside position
    path.push_back({ tx, tz });

    return path;
}

// ============================================================================
// INITIALIZATION OF SIMULATOR STATE
// ============================================================================

void initHospitalLayout() {
    beds.clear();
    doctors.clear();
    walkingPatients.clear();
    nextPatientId = 101;
    totalAdmitted = 0;
    totalDischarged = 0;
    surgeModeActive = false;

    // 1. RED ZONE (Critical Care / Resuscitation) - West Wing
    beds.push_back({ 1, -18.0f, -6.0f,  90.0f, ZONE_RED, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 2, -18.0f,  0.0f,  90.0f, ZONE_RED, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 3, -18.0f,  6.0f,  90.0f, ZONE_RED, false, 0,                0.0f, false });
    totalAdmitted += 2;

    // 2. YELLOW ZONE (Urgent Care) - North-West Wing
    beds.push_back({ 4, -16.0f, -16.0f, 0.0f, ZONE_YELLOW, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 5, -10.0f, -16.0f, 0.0f, ZONE_YELLOW, false, 0,                0.0f, false });
    totalAdmitted += 1;

    // 3. GREEN ZONE (Minor Injuries / Ambulatory) - East Wing
    beds.push_back({ 6, 18.0f, -6.0f, -90.0f, ZONE_GREEN, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 7, 18.0f,  0.0f, -90.0f, ZONE_GREEN, false, 0,                0.0f, false });
    beds.push_back({ 8, 18.0f,  6.0f, -90.0f, ZONE_GREEN, false, 0,                0.0f, false });
    totalAdmitted += 1;

    // 4. BLUE ZONE (Observation / Diagnostics) - North-East Wing
    beds.push_back({ 9,  10.0f, -16.0f, 0.0f, ZONE_BLUE, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 10, 16.0f, -16.0f, 0.0f, ZONE_BLUE, false, 0,                0.0f, false });
    totalAdmitted += 1;

    // 5. PURPLE ZONE (Specialized Treatment / Trauma Prep) - North-Center
    beds.push_back({ 11, -3.0f, -16.0f, 0.0f, ZONE_PURPLE, true,  nextPatientId++, 0.0f, false });
    beds.push_back({ 12,  3.0f, -16.0f, 0.0f, ZONE_PURPLE, false, 0,                0.0f, false });
    totalAdmitted += 1;

    // 6. WHITE ZONE (General Support & Recovery) - South-East Wing
    beds.push_back({ 13, 18.0f, 13.0f, -90.0f, ZONE_WHITE, false, 0, 0.0f, false });
    beds.push_back({ 14, 12.0f, 13.0f, -90.0f, ZONE_WHITE, false, 0, 0.0f, false });

    // 7. ORANGE ZONE (Emergency Overflow Surge) - South-West Wing
    beds.push_back({ 15, -18.0f, 13.0f, 90.0f, ZONE_ORANGE, false, 0, 0.0f, false });
    beds.push_back({ 16, -12.0f, 13.0f, 90.0f, ZONE_ORANGE, false, 0, 0.0f, false });

    // DOCTOR FLEET (1 Assigned Doctor per Zone + Mobile Support)
    // Doctor 1: Red Zone ICU Specialist
    doctors.push_back({ 1, -12.0f, -2.0f, -12.0f, -2.0f, -12.0f, -2.0f, {}, 0, 0.065f, 0.0f, 0.0f, true, ZONE_RED, ZONE_RED, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 2: Yellow Zone Physician
    doctors.push_back({ 2, -13.0f, -11.0f, -13.0f, -11.0f, -13.0f, -11.0f, {}, 0, 0.060f, 0.0f, 0.0f, true, ZONE_YELLOW, ZONE_YELLOW, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 3: Green Zone Triage Officer
    doctors.push_back({ 3, 12.0f, 0.0f, 12.0f, 0.0f, 12.0f, 0.0f, {}, 0, 0.060f, 0.0f, 0.0f, true, ZONE_GREEN, ZONE_GREEN, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 4: Blue Zone Diagnostic Specialist
    doctors.push_back({ 4, 13.0f, -11.0f, 13.0f, -11.0f, 13.0f, -11.0f, {}, 0, 0.060f, 0.0f, 0.0f, true, ZONE_BLUE, ZONE_BLUE, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 5: Purple Zone Trauma Surgeon
    doctors.push_back({ 5, 0.0f, -11.0f, 0.0f, -11.0f, 0.0f, -11.0f, {}, 0, 0.065f, 0.0f, 0.0f, true, ZONE_PURPLE, ZONE_PURPLE, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 6: White Zone Support Clinician
    doctors.push_back({ 6, 8.0f, 10.0f, 8.0f, 10.0f, 8.0f, 10.0f, {}, 0, 0.055f, 0.0f, 0.0f, true, ZONE_WHITE, ZONE_WHITE, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
    // Doctor 7: Orange Zone Rapid-Response Officer
    doctors.push_back({ 7, -8.0f, 10.0f, -8.0f, 10.0f, -8.0f, 10.0f, {}, 0, 0.065f, 0.0f, 0.0f, true, ZONE_ORANGE, ZONE_ORANGE, DOC_IDLE_PATROL, -1, 0.0f, 0.0f, false });
}

// Zone Metrics
int getOccupiedCount(ZoneType z) {
    int count = 0;
    for (size_t i = 0; i < beds.size(); i++) {
        if (beds[i].zone == z && beds[i].occupied) count++;
    }
    return count;
}

int getTotalCount(ZoneType z) {
    int count = 0;
    for (size_t i = 0; i < beds.size(); i++) {
        if (beds[i].zone == z) count++;
    }
    return count;
}

int getAvailableDoctorCount(ZoneType z) {
    int count = 0;
    for (size_t i = 0; i < doctors.size(); i++) {
        if (doctors[i].assignedZone == z && doctors[i].state == DOC_IDLE_PATROL) count++;
    }
    return count;
}

// Find a doctor in a zone to treat a patient
void dispatchDoctorToBed(int bedIndex) {
    ZoneType z = beds[bedIndex].zone;

    // Priority 1: Check doctor assigned to this zone
    for (size_t i = 0; i < doctors.size(); i++) {
        if (doctors[i].assignedZone == z && doctors[i].state == DOC_IDLE_PATROL) {
            float bedSideX = beds[bedIndex].x + ((beds[bedIndex].rotation == 90.0f) ? 2.5f : ((beds[bedIndex].rotation == -90.0f) ? -2.5f : 0.0f));
            float bedSideZ = beds[bedIndex].z + ((beds[bedIndex].rotation == 0.0f) ? 2.5f : 0.0f);

            doctors[i].treatingBedIndex = bedIndex;
            doctors[i].targetX = bedSideX;
            doctors[i].targetZ = bedSideZ;
            doctors[i].path = buildCorridorPath(doctors[i].x, doctors[i].z, bedSideX, bedSideZ);
            doctors[i].pathIndex = 0;
            doctors[i].state = DOC_MOVING_TO_PATIENT;
            return;
        }
    }

    // Priority 2: If none in zone, any idle doctor from White or Orange support
    for (size_t i = 0; i < doctors.size(); i++) {
        if ((doctors[i].assignedZone == ZONE_WHITE || doctors[i].assignedZone == ZONE_ORANGE) &&
            doctors[i].state == DOC_IDLE_PATROL) {
            float bedSideX = beds[bedIndex].x + ((beds[bedIndex].rotation == 90.0f) ? 2.5f : ((beds[bedIndex].rotation == -90.0f) ? -2.5f : 0.0f));
            float bedSideZ = beds[bedIndex].z + ((beds[bedIndex].rotation == 0.0f) ? 2.5f : 0.0f);

            doctors[i].treatingBedIndex = bedIndex;
            doctors[i].targetX = bedSideX;
            doctors[i].targetZ = bedSideZ;
            doctors[i].path = buildCorridorPath(doctors[i].x, doctors[i].z, bedSideX, bedSideZ);
            doctors[i].pathIndex = 0;
            doctors[i].state = DOC_MOVING_TO_PATIENT;
            return;
        }
    }
}

// Dynamic Resource Allocation: Reallocate beds and staff when Red Zone reaches capacity
bool checkAndReallocateResources() {
    int redOcc = getOccupiedCount(ZONE_RED);
    int redTot = getTotalCount(ZONE_RED);

    if (redOcc >= redTot) {
        // Red zone is full! Search for available bed in Orange (Overflow) or White (Support)
        ZoneType donorZones[] = { ZONE_ORANGE, ZONE_WHITE, ZONE_GREEN };
        for (int d = 0; d < 3; d++) {
            for (size_t i = 0; i < beds.size(); i++) {
                if (beds[i].zone == donorZones[d] && !beds[i].occupied && !beds[i].isReallocated) {
                    beds[i].zone = ZONE_RED;
                    beds[i].isReallocated = true;
                    surgeModeActive = true;

                    // Also reallocate an idle doctor to Red Zone
                    for (size_t docIdx = 0; docIdx < doctors.size(); docIdx++) {
                        if (doctors[docIdx].assignedZone == donorZones[d] && !doctors[docIdx].isReallocated) {
                            doctors[docIdx].assignedZone = ZONE_RED;
                            doctors[docIdx].isReallocated = true;
                            break;
                        }
                    }

                    char msg[140];
                    sprintf(msg, "[DYNAMIC SURGE] Red Zone full! Bed #%d & Doctor reallocated from %s to RED Zone.",
                            beds[i].id, ZONE_NAMES[donorZones[d]]);
                    statusMessage = msg;
                    return true;
                }
            }
        }
    }
    return false;
}

// Admit Patient with Pathfinding Animation
bool admitPatient(ZoneType preferredZone, bool fromAmbulance = false) {
    // 1. Check if dynamic resource allocation needed for Red Zone
    if (preferredZone == ZONE_RED && getOccupiedCount(ZONE_RED) >= getTotalCount(ZONE_RED)) {
        checkAndReallocateResources();
    }

    // 2. Find available bed in preferred zone
    int targetBedIdx = -1;
    for (size_t i = 0; i < beds.size(); i++) {
        if (beds[i].zone == preferredZone && !beds[i].occupied) {
            // Also check no walking patient is already heading to this bed
            bool claimed = false;
            for (size_t p = 0; p < walkingPatients.size(); p++) {
                if (walkingPatients[p].targetBedIndex == (int)i) {
                    claimed = true;
                    break;
                }
            }
            if (!claimed) {
                targetBedIdx = (int)i;
                break;
            }
        }
    }

    // If preferred zone is completely full, try overflow zone
    if (targetBedIdx == -1 && preferredZone != ZONE_ORANGE) {
        for (size_t i = 0; i < beds.size(); i++) {
            if (beds[i].zone == ZONE_ORANGE && !beds[i].occupied) {
                targetBedIdx = (int)i;
                preferredZone = ZONE_ORANGE;
                break;
            }
        }
    }

    if (targetBedIdx == -1) {
        char msg[128];
        sprintf(msg, "ALERT: %s Zone is AT FULL CAPACITY! Bed allocation failed.", ZONE_NAMES[preferredZone]);
        statusMessage = msg;
        return false;
    }

    // 3. Spawn Walking Patient
    WalkingPatient p;
    p.id = nextPatientId++;
    p.assignedZone = preferredZone;
    p.targetBedIndex = targetBedIdx;
    p.speed = 0.08f;
    p.legAngle = 0.0f;
    p.legForward = true;
    p.arrived = false;

    // Patient clothing color (triage themed)
    getZoneColor(preferredZone, p.rColor, p.gColor, p.bColor);
    // Tint slightly pastel
    p.rColor = p.rColor * 0.7f + 0.25f;
    p.gColor = p.gColor * 0.7f + 0.25f;
    p.bColor = p.bColor * 0.7f + 0.25f;

    // Start coordinates
    float startX, startZ;
    if (fromAmbulance) {
        startX = 14.0f;
        startZ = 22.0f; // Rear of docked ambulance
    } else {
        startX = 4.0f;
        startZ = 18.0f; // Main hospital entrance
    }

    p.x = startX;
    p.z = startZ;

    // Target bedside position
    float targetX = beds[targetBedIdx].x + ((beds[targetBedIdx].rotation == 90.0f) ? 2.5f : ((beds[targetBedIdx].rotation == -90.0f) ? -2.5f : 0.0f));
    float targetZ = beds[targetBedIdx].z + ((beds[targetBedIdx].rotation == 0.0f) ? 2.5f : 0.0f);

    p.path = buildCorridorPath(startX, startZ, targetX, targetZ);
    p.pathIndex = 0;
    p.rotation = 180.0f;

    walkingPatients.push_back(p);
    totalAdmitted++;

    char msg[128];
    sprintf(msg, "Patient #%d entered ED -> Walking to %s Zone Bed #%d.",
            p.id, ZONE_NAMES[preferredZone], beds[targetBedIdx].id);
    statusMessage = msg;

    return true;
}

// Discharge Patient
bool dischargePatient() {
    ZoneType searchOrder[] = { ZONE_GREEN, ZONE_WHITE, ZONE_BLUE, ZONE_YELLOW, ZONE_PURPLE, ZONE_ORANGE, ZONE_RED };
    for (int s = 0; s < NUM_ZONES; s++) {
        for (size_t i = 0; i < beds.size(); i++) {
            if (beds[i].zone == searchOrder[s] && beds[i].occupied) {
                beds[i].occupied = false;
                totalDischarged++;

                // If bed was reallocated to Red, revert it back if Red is calm
                if (beds[i].isReallocated && getOccupiedCount(ZONE_RED) < 3) {
                    beds[i].isReallocated = false;
                    beds[i].zone = (beds[i].id >= 15) ? ZONE_ORANGE : ZONE_WHITE;
                }

                char msg[128];
                sprintf(msg, "Patient #%d discharged. Bed #%d freed in %s Zone.",
                        beds[i].patientId, beds[i].id, ZONE_NAMES[searchOrder[s]]);
                statusMessage = msg;
                return true;
            }
        }
    }
    statusMessage = "All beds are currently empty. No patients to discharge.";
    return false;
}

// ============================================================================
// 3D MODELING FUNCTIONS
// ============================================================================

// Draw Humanoid Character (Doctor, Walking Patient, or Patient lying on bed)
void drawHuman(bool isDoctor, bool isLyingDown, float legAngle, float armAngle, float rColor, float gColor, float bColor) {
    glPushMatrix();

    if (isLyingDown) {
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, 0.45f, -0.6f);
    }

    // 1. Torso / Coat
    setMaterial(rColor, gColor, bColor, 20.0f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, 1.25f, 0.0f);
    drawSolidBox(0.75f, 0.95f, 0.45f);
    glPopMatrix();

    // 2. Doctor Stethoscope & ID Badge
    if (isDoctor) {
        setMaterial(0.2f, 0.2f, 0.25f);
        glPushMatrix();
        glTranslatef(0.0f, 1.6f, 0.24f);
        drawSolidBox(0.4f, 0.15f, 0.05f);
        glPopMatrix();
    }

    // 3. Head & Face
    setMaterial(0.96f, 0.80f, 0.69f, 16.0f, 0.1f);
    glPushMatrix();
    glTranslatef(0.0f, 2.05f, 0.0f);
    glutSolidSphere(0.32f, 16, 16);

    // Surgical Cap
    if (isDoctor) {
        setMaterial(0.15f, 0.55f, 0.75f);
        glTranslatef(0.0f, 0.18f, 0.0f);
        glutSolidSphere(0.25f, 14, 14);
    }
    glPopMatrix();

    if (!isLyingDown) {
        // 4. Arms
        setMaterial(rColor, gColor, bColor);
        // Left Arm
        glPushMatrix();
        glTranslatef(-0.48f, 1.6f, 0.0f);
        glRotatef(legAngle * 0.7f + armAngle, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.4f, 0.0f);
        drawSolidBox(0.2f, 0.75f, 0.2f);
        glPopMatrix();

        // Right Arm (Treatment examination motion when armAngle is active)
        glPushMatrix();
        glTranslatef(0.48f, 1.6f, 0.0f);
        glRotatef(-legAngle * 0.7f - armAngle * 1.5f, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.4f, 0.0f);
        drawSolidBox(0.2f, 0.75f, 0.2f);
        glPopMatrix();

        // 5. Legs
        setMaterial(0.2f, 0.25f, 0.4f);
        // Left Leg
        glPushMatrix();
        glTranslatef(-0.22f, 0.8f, 0.0f);
        glRotatef(legAngle, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.4f, 0.0f);
        drawSolidBox(0.24f, 0.8f, 0.24f);
        // Shoe
        setMaterial(0.1f, 0.1f, 0.1f);
        glTranslatef(0.0f, -0.42f, 0.06f);
        drawSolidBox(0.25f, 0.14f, 0.35f);
        glPopMatrix();

        // Right Leg
        glPushMatrix();
        glTranslatef(0.22f, 0.8f, 0.0f);
        glRotatef(-legAngle, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.4f, 0.0f);
        setMaterial(0.2f, 0.25f, 0.4f);
        drawSolidBox(0.24f, 0.8f, 0.24f);
        // Shoe
        setMaterial(0.1f, 0.1f, 0.1f);
        glTranslatef(0.0f, -0.42f, 0.06f);
        drawSolidBox(0.25f, 0.14f, 0.35f);
        glPopMatrix();
    }

    glPopMatrix();
}

// Draw Treatment Aura / Medical Assessment Indicator
void drawTreatmentIndicator(float x, float y, float z, float pulse) {
    glPushMatrix();
    glTranslatef(x, y + 2.4f, z);
    glRotatef(pulse * 60.0f, 0.0f, 1.0f, 0.0f);

    // Glowing cyan/green medical cross above patient
    setMaterial(0.1f, 0.95f, 0.75f, 128.0f, 0.9f);
    drawSolidBox(0.6f, 0.18f, 0.12f);
    drawSolidBox(0.18f, 0.6f, 0.12f);

    glPopMatrix();
}

// Draw Hospital Bed
void drawHospitalBed(const Bed &bed, float conditionPulse) {
    glPushMatrix();
    glTranslatef(bed.x, 0.0f, bed.z);
    glRotatef(bed.rotation, 0.0f, 1.0f, 0.0f);

    // 1. Bed Frame
    setMaterial(0.35f, 0.38f, 0.42f, 64.0f, 0.6f);
    glPushMatrix();
    glTranslatef(0.0f, 0.55f, 0.0f);
    drawSolidBox(1.8f, 0.15f, 3.4f);
    glPopMatrix();

    // Legs & Wheels
    for (float lx = -0.8f; lx <= 0.8f; lx += 1.6f) {
        for (float lz = -1.6f; lz <= 1.6f; lz += 3.2f) {
            glPushMatrix();
            glTranslatef(lx, 0.0f, lz);
            drawCylinderY(0.05f, 0.55f);
            setMaterial(0.15f, 0.15f, 0.15f);
            glTranslatef(0.0f, 0.05f, 0.0f);
            glutSolidSphere(0.08f, 8, 8);
            setMaterial(0.35f, 0.38f, 0.42f);
            glPopMatrix();
        }
    }

    // Headboard & Footboard
    glPushMatrix();
    glTranslatef(0.0f, 0.6f, -1.65f);
    drawSolidBox(1.8f, 0.9f, 0.1f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.6f, 1.65f);
    drawSolidBox(1.8f, 0.6f, 0.1f);
    glPopMatrix();

    // Guard Rails
    for (float rx = -0.9f; rx <= 0.9f; rx += 1.8f) {
        glPushMatrix();
        glTranslatef(rx, 0.7f, 0.0f);
        drawSolidBox(0.05f, 0.35f, 2.2f);
        glPopMatrix();
    }

    // 2. Mattress
    setMaterial(0.92f, 0.93f, 0.95f, 10.0f, 0.1f);
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    drawSolidBox(1.65f, 0.28f, 3.2f);
    glPopMatrix();

    // 3. Pillow
    setMaterial(0.85f, 0.90f, 0.98f);
    glPushMatrix();
    glTranslatef(0.0f, 0.92f, -1.25f);
    drawSolidBox(1.2f, 0.16f, 0.6f);
    glPopMatrix();

    // 4. Patient or Folded Blanket
    if (bed.occupied) {
        glPushMatrix();
        glTranslatef(0.0f, 0.45f, 0.1f);
        float pr, pg, pb;
        getZoneColor(bed.zone, pr, pg, pb);
        drawHuman(false, true, 0.0f, 0.0f, pr * 0.75f + 0.2f, pg * 0.75f + 0.2f, pb * 0.75f + 0.2f);
        glPopMatrix();

        // Blanket
        setMaterial(0.80f, 0.88f, 0.92f);
        glPushMatrix();
        glTranslatef(0.0f, 0.96f, 0.25f);
        drawSolidBox(1.5f, 0.22f, 2.1f);
        glPopMatrix();
    } else {
        setMaterial(0.70f, 0.80f, 0.85f);
        glPushMatrix();
        glTranslatef(0.0f, 0.88f, 0.8f);
        drawSolidBox(1.4f, 0.10f, 1.0f);
        glPopMatrix();
    }

    // 5. IV Drip Stand
    glPushMatrix();
    glTranslatef(-1.25f, 0.0f, -1.0f);
    setMaterial(0.3f, 0.3f, 0.35f);
    drawCylinderY(0.03f, 2.8f);
    glPushMatrix();
    glTranslatef(0.0f, 2.75f, 0.0f);
    drawSolidBox(0.4f, 0.03f, 0.03f);
    setMaterial(0.85f, 0.95f, 1.0f, 128.0f, 0.8f);
    glTranslatef(0.15f, -0.25f, 0.0f);
    drawSolidBox(0.12f, 0.28f, 0.08f);
    glPopMatrix();
    glPopMatrix();

    // 6. Patient Vitals Monitor Screen
    glPushMatrix();
    glTranslatef(1.25f, 0.0f, -1.1f);
    setMaterial(0.25f, 0.25f, 0.3f);
    drawCylinderY(0.04f, 2.0f);
    glTranslatef(0.0f, 2.0f, 0.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    setMaterial(0.2f, 0.2f, 0.25f);
    drawSolidBox(0.7f, 0.5f, 0.2f);

    // Screen
    if (bed.occupied) {
        float mr, mg, mb;
        getZoneColor(bed.zone, mr, mg, mb);
        if (bed.zone == ZONE_RED) {
            setMaterial(0.8f + 0.2f * sin(conditionPulse), 0.1f, 0.1f);
        } else {
            setMaterial(mr, mg, mb);
        }
    } else {
        setMaterial(0.05f, 0.05f, 0.08f);
    }
    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.11f);
    drawSolidBox(0.58f, 0.38f, 0.02f);
    glPopMatrix();
    glPopMatrix();

    glPopMatrix();
}

// Draw Ambulance with Flashing Siren Beacon
void drawAmbulance(float x, float z, float angle, bool sirenOn, bool beacon) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    // Chassis
    setMaterial(0.15f, 0.15f, 0.18f);
    glPushMatrix();
    glTranslatef(0.0f, 0.45f, 0.0f);
    drawSolidBox(2.6f, 0.4f, 6.2f);
    glPopMatrix();

    // Body
    setMaterial(0.95f, 0.95f, 0.97f, 64.0f, 0.5f);
    glPushMatrix();
    glTranslatef(0.0f, 1.6f, 0.3f);
    drawSolidBox(2.55f, 1.9f, 4.6f);
    glPopMatrix();

    // Cabin
    glPushMatrix();
    glTranslatef(0.0f, 1.4f, -2.4f);
    drawSolidBox(2.5f, 1.5f, 1.7f);
    glPopMatrix();

    // Windshield
    setMaterial(0.2f, 0.35f, 0.55f, 128.0f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 1.65f, -3.26f);
    glRotatef(-15.0f, 1.0f, 0.0f, 0.0f);
    drawSolidBox(2.2f, 0.85f, 0.05f);
    glPopMatrix();

    // Windows
    for (float wx = -1.29f; wx <= 1.29f; wx += 2.58f) {
        glPushMatrix();
        glTranslatef(wx, 1.75f, 0.3f);
        drawSolidBox(0.04f, 0.7f, 3.2f);
        glPopMatrix();
    }

    // Red Emergency Stripe & Cross
    setMaterial(0.85f, 0.12f, 0.12f);
    for (float sx = -1.30f; sx <= 1.30f; sx += 2.60f) {
        glPushMatrix();
        glTranslatef(sx, 1.3f, 0.3f);
        drawSolidBox(0.02f, 0.35f, 4.5f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(sx, 1.9f, 0.3f);
        drawSolidBox(0.03f, 0.6f, 0.2f);
        drawSolidBox(0.03f, 0.2f, 0.6f);
        glPopMatrix();
    }

    // Wheels
    for (float wx = -1.35f; wx <= 1.35f; wx += 2.7f) {
        for (float wz = -1.8f; wz <= 1.8f; wz += 3.6f) {
            glPushMatrix();
            glTranslatef(wx, 0.45f, wz);
            glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
            setMaterial(0.1f, 0.1f, 0.12f);
            glutSolidTorus(0.18f, 0.32f, 12, 16);
            setMaterial(0.75f, 0.75f, 0.8f, 96.0f, 0.8f);
            glutSolidSphere(0.22f, 10, 10);
            glPopMatrix();
        }
    }

    // Rooftop Siren Lightbar
    setMaterial(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, 2.62f, -1.8f);
    drawSolidBox(1.5f, 0.12f, 0.35f);

    // Left Beacon (Red)
    if (sirenOn && beacon) {
        setMaterial(1.0f, 0.1f, 0.1f, 128.0f, 1.0f);
    } else {
        setMaterial(0.4f, 0.05f, 0.05f);
    }
    glPushMatrix();
    glTranslatef(-0.45f, 0.18f, 0.0f);
    drawSolidBox(0.45f, 0.24f, 0.28f);
    glPopMatrix();

    // Right Beacon (Blue)
    if (sirenOn && !beacon) {
        setMaterial(0.1f, 0.4f, 1.0f, 128.0f, 1.0f);
    } else {
        setMaterial(0.05f, 0.1f, 0.4f);
    }
    glPushMatrix();
    glTranslatef(0.45f, 0.18f, 0.0f);
    drawSolidBox(0.45f, 0.24f, 0.28f);
    glPopMatrix();
    glPopMatrix();

    // Headlights
    setMaterial(1.0f, 0.98f, 0.7f, 100.0f, 1.0f);
    glPushMatrix();
    glTranslatef(-0.95f, 0.75f, -3.27f);
    glutSolidSphere(0.16f, 10, 10);
    glTranslatef(1.90f, 0.0f, 0.0f);
    glutSolidSphere(0.16f, 10, 10);
    glPopMatrix();

    glPopMatrix();
}

// Draw Reception & Triage Station
void drawTriageStation(float x, float z) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);

    setMaterial(0.85f, 0.72f, 0.55f);
    glPushMatrix();
    glTranslatef(0.0f, 0.55f, 0.0f);
    drawSolidBox(5.0f, 1.1f, 1.6f);
    glPopMatrix();

    setMaterial(0.95f, 0.95f, 0.98f, 64.0f, 0.6f);
    glPushMatrix();
    glTranslatef(0.0f, 1.12f, 0.0f);
    drawSolidBox(5.2f, 0.08f, 1.8f);
    glPopMatrix();

    // Computer
    setMaterial(0.15f, 0.15f, 0.2f);
    glPushMatrix();
    glTranslatef(-1.2f, 1.16f, 0.0f);
    drawCylinderY(0.04f, 0.35f);
    glTranslatef(0.0f, 0.35f, 0.0f);
    drawSolidBox(0.6f, 0.42f, 0.08f);
    setMaterial(0.2f, 0.6f, 0.85f);
    glTranslatef(0.0f, 0.21f, 0.05f);
    drawSolidBox(0.52f, 0.34f, 0.02f);
    glPopMatrix();

    // Triage Nurse
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.9f);
    drawHuman(true, false, 0.0f, 0.0f, 0.92f, 0.95f, 0.98f);
    glPopMatrix();

    // Overhead Sign
    setMaterial(0.2f, 0.4f, 0.7f);
    glPushMatrix();
    glTranslatef(0.0f, 3.2f, 0.0f);
    drawSolidBox(3.4f, 0.6f, 0.1f);
    glPopMatrix();

    glPopMatrix();
}

// ============================================================================
// ENVIRONMENT & HOSPITAL ARCHITECTURE (ALL 7 ZONES)
// ============================================================================

void drawHospitalArchitecture() {
    // 1. Hospital Main Floor
    glBegin(GL_QUADS);
    for (int x = -24; x < 24; x += 2) {
        for (int z = -22; z < 18; z += 2) {
            if ((x / 2 + z / 2) % 2 == 0) {
                glNormal3f(0.0f, 1.0f, 0.0f);
                setMaterial(0.92f, 0.94f, 0.96f);
            } else {
                glNormal3f(0.0f, 1.0f, 0.0f);
                setMaterial(0.88f, 0.90f, 0.92f);
            }
            glVertex3f((float)x, 0.0f, (float)z);
            glVertex3f((float)x + 2.0f, 0.0f, (float)z);
            glVertex3f((float)x + 2.0f, 0.0f, (float)z + 2.0f);
            glVertex3f((float)x, 0.0f, (float)z + 2.0f);
        }
    }
    glEnd();

    // 2. Outdoor Ambulance Driveway (Z = 18 to 38)
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    setMaterial(0.28f, 0.30f, 0.33f);
    glVertex3f(-26.0f, -0.01f, 18.0f);
    glVertex3f( 26.0f, -0.01f, 18.0f);
    glVertex3f( 26.0f, -0.01f, 38.0f);
    glVertex3f(-26.0f, -0.01f, 38.0f);
    glEnd();

    // Driveway markings
    setMaterial(0.92f, 0.85f, 0.1f);
    for (float rz = 20.0f; rz <= 34.0f; rz += 4.0f) {
        glPushMatrix();
        glTranslatef(14.0f, 0.02f, rz);
        drawSolidBox(0.35f, 0.02f, 2.0f);
        glPopMatrix();
    }

    // 3. COLOR-CODED TRIAGE ZONES (FLOOR PADS & OVERHEAD SIGNS)
    // ------------------------------------------------------------------------
    // (1) RED ZONE (Critical Care) - West Wing
    setMaterial(0.88f, 0.18f, 0.18f);
    glPushMatrix();
    glTranslatef(-17.5f, 0.01f, 0.0f);
    drawSolidBox(11.0f, 0.02f, 17.0f);
    glPopMatrix();

    setMaterial(0.96f, 0.88f, 0.88f);
    glPushMatrix();
    glTranslatef(-17.5f, 0.015f, 0.0f);
    drawSolidBox(10.4f, 0.02f, 16.4f);
    glPopMatrix();

    // Red Sign
    setMaterial(0.88f, 0.18f, 0.18f);
    glPushMatrix();
    glTranslatef(-17.5f, 4.2f, 8.6f);
    drawSolidBox(6.5f, 0.8f, 0.2f);
    glPopMatrix();

    // (2) YELLOW ZONE (Urgent Care) - North-West
    setMaterial(0.90f, 0.78f, 0.12f);
    glPushMatrix();
    glTranslatef(-13.0f, 0.01f, -15.5f);
    drawSolidBox(10.0f, 0.02f, 9.0f);
    glPopMatrix();

    setMaterial(0.97f, 0.95f, 0.85f);
    glPushMatrix();
    glTranslatef(-13.0f, 0.015f, -15.5f);
    drawSolidBox(9.4f, 0.02f, 8.4f);
    glPopMatrix();

    // Yellow Sign
    setMaterial(0.90f, 0.78f, 0.12f);
    glPushMatrix();
    glTranslatef(-13.0f, 4.2f, -11.0f);
    drawSolidBox(5.5f, 0.8f, 0.2f);
    glPopMatrix();

    // (3) GREEN ZONE (Minor Care) - East Wing
    setMaterial(0.18f, 0.76f, 0.32f);
    glPushMatrix();
    glTranslatef(17.5f, 0.01f, 0.0f);
    drawSolidBox(11.0f, 0.02f, 17.0f);
    glPopMatrix();

    setMaterial(0.88f, 0.96f, 0.90f);
    glPushMatrix();
    glTranslatef(17.5f, 0.015f, 0.0f);
    drawSolidBox(10.4f, 0.02f, 16.4f);
    glPopMatrix();

    // Green Sign
    setMaterial(0.18f, 0.76f, 0.32f);
    glPushMatrix();
    glTranslatef(17.5f, 4.2f, 8.6f);
    drawSolidBox(6.5f, 0.8f, 0.2f);
    glPopMatrix();

    // (4) BLUE ZONE (Observation) - North-East
    setMaterial(0.18f, 0.46f, 0.88f);
    glPushMatrix();
    glTranslatef(13.0f, 0.01f, -15.5f);
    drawSolidBox(10.0f, 0.02f, 9.0f);
    glPopMatrix();

    setMaterial(0.88f, 0.92f, 0.98f);
    glPushMatrix();
    glTranslatef(13.0f, 0.015f, -15.5f);
    drawSolidBox(9.4f, 0.02f, 8.4f);
    glPopMatrix();

    // Blue Sign
    setMaterial(0.18f, 0.46f, 0.88f);
    glPushMatrix();
    glTranslatef(13.0f, 4.2f, -11.0f);
    drawSolidBox(5.5f, 0.8f, 0.2f);
    glPopMatrix();

    // (5) PURPLE ZONE (Specialized Treatment / Trauma) - North-Center
    setMaterial(0.65f, 0.18f, 0.82f);
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, -15.5f);
    drawSolidBox(12.0f, 0.02f, 9.0f);
    glPopMatrix();

    setMaterial(0.95f, 0.88f, 0.98f);
    glPushMatrix();
    glTranslatef(0.0f, 0.015f, -15.5f);
    drawSolidBox(11.4f, 0.02f, 8.4f);
    glPopMatrix();

    // Purple Sign
    setMaterial(0.65f, 0.18f, 0.82f);
    glPushMatrix();
    glTranslatef(0.0f, 4.2f, -11.0f);
    drawSolidBox(7.0f, 0.8f, 0.2f);
    glPopMatrix();

    // (6) WHITE ZONE (General Support & Recovery) - South-East
    setMaterial(0.85f, 0.88f, 0.92f);
    glPushMatrix();
    glTranslatef(15.0f, 0.01f, 13.0f);
    drawSolidBox(11.0f, 0.02f, 7.0f);
    glPopMatrix();

    setMaterial(0.98f, 0.98f, 0.99f);
    glPushMatrix();
    glTranslatef(15.0f, 0.015f, 13.0f);
    drawSolidBox(10.4f, 0.02f, 6.4f);
    glPopMatrix();

    // White Sign
    setMaterial(0.55f, 0.60f, 0.65f);
    glPushMatrix();
    glTranslatef(15.0f, 4.2f, 16.6f);
    drawSolidBox(6.5f, 0.8f, 0.2f);
    glPopMatrix();

    // (7) ORANGE ZONE (Emergency Overflow Surge) - South-West
    setMaterial(0.95f, 0.48f, 0.10f);
    glPushMatrix();
    glTranslatef(-15.0f, 0.01f, 13.0f);
    drawSolidBox(11.0f, 0.02f, 7.0f);
    glPopMatrix();

    setMaterial(0.98f, 0.92f, 0.85f);
    glPushMatrix();
    glTranslatef(-15.0f, 0.015f, 13.0f);
    drawSolidBox(10.4f, 0.02f, 6.4f);
    glPopMatrix();

    // Orange Sign
    setMaterial(0.95f, 0.48f, 0.10f);
    glPushMatrix();
    glTranslatef(-15.0f, 4.2f, 16.6f);
    drawSolidBox(6.5f, 0.8f, 0.2f);
    glPopMatrix();

    // 4. HOSPITAL WALLS
    // North Wall (Z = -22)
    setMaterial(0.82f, 0.86f, 0.90f);
    glPushMatrix();
    glTranslatef(0.0f, 2.5f, -22.0f);
    drawSolidBox(48.0f, 5.0f, 0.6f);
    glPopMatrix();

    // West Outer Wall (X = -24)
    glPushMatrix();
    glTranslatef(-24.0f, 2.5f, -2.0f);
    drawSolidBox(0.6f, 5.0f, 40.0f);
    glPopMatrix();

    // East Outer Wall (X = +24)
    glPushMatrix();
    glTranslatef(24.0f, 2.5f, -2.0f);
    drawSolidBox(0.6f, 5.0f, 40.0f);
    glPopMatrix();

    // South Front Wall with Emergency Entrance Opening
    glPushMatrix();
    glTranslatef(-16.0f, 2.5f, 18.0f);
    drawSolidBox(16.0f, 5.0f, 0.6f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(18.0f, 2.5f, 18.0f);
    drawSolidBox(12.0f, 5.0f, 0.6f);
    glPopMatrix();

    // Glass Sliding Doors
    setMaterial(0.4f, 0.65f, 0.85f, 128.0f, 0.9f);
    glPushMatrix();
    glTranslatef(4.0f, 2.0f, 18.0f);
    drawSolidBox(12.0f, 4.0f, 0.15f);
    glPopMatrix();

    // Internal Partitions
    setMaterial(0.78f, 0.82f, 0.86f);
    // Red Zone Partition
    glPushMatrix();
    glTranslatef(-12.0f, 2.0f, 0.0f);
    drawSolidBox(0.4f, 4.0f, 16.0f);
    glPopMatrix();

    // Green Zone Partition
    glPushMatrix();
    glTranslatef(12.0f, 2.0f, 0.0f);
    drawSolidBox(0.4f, 4.0f, 16.0f);
    glPopMatrix();

    // North Zone Partitions
    glPushMatrix();
    glTranslatef(-6.5f, 2.0f, -15.5f);
    drawSolidBox(0.4f, 4.0f, 9.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(6.5f, 2.0f, -15.5f);
    drawSolidBox(0.4f, 4.0f, 9.0f);
    glPopMatrix();

    // Main Overhead Entrance Canopy
    setMaterial(0.85f, 0.15f, 0.15f);
    glPushMatrix();
    glTranslatef(4.0f, 4.6f, 18.3f);
    drawSolidBox(12.0f, 0.9f, 0.3f);
    glPopMatrix();
}

// ============================================================================
// 2D DASHBOARD & HUD OVERLAY
// ============================================================================

void renderHUD() {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 1. TOP HEADER BANNER
    glColor4f(0.08f, 0.12f, 0.18f, 0.88f);
    glBegin(GL_QUADS);
    glVertex2i(0, windowHeight);
    glVertex2i(windowWidth, windowHeight);
    glVertex2i(windowWidth, windowHeight - 65);
    glVertex2i(0, windowHeight - 65);
    glEnd();

    // Header Accent Line
    glColor3f(0.2f, 0.6f, 0.95f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2i(0, windowHeight - 65);
    glVertex2i(windowWidth, windowHeight - 65);
    glEnd();

    // Title Text
    glColor3f(1.0f, 1.0f, 1.0f);
    renderBitmapString(25, windowHeight - 26, GLUT_BITMAP_HELVETICA_18,
                       "3D CROWD EMERGENCY MANAGEMENT & RESOURCE ALLOCATION SIMULATOR");
    glColor3f(0.7f, 0.85f, 0.95f);
    renderBitmapString(25, windowHeight - 48, GLUT_BITMAP_HELVETICA_12,
                       "Multi-Zone Hospital ED | Dynamic Staff & Bed Reallocation | Patient Pathfinding & Doctor Treatment");

    // 2. RESOURCE ALLOCATION STATUS PANEL (Top Right - Comprehensive 7-Zone Table)
    int panelW = 430;
    int panelH = 265;
    int panelX = windowWidth - panelW - 20;
    int panelY = windowHeight - 75;

    glColor4f(0.05f, 0.08f, 0.12f, 0.88f);
    glBegin(GL_QUADS);
    glVertex2i(panelX, panelY);
    glVertex2i(panelX + panelW, panelY);
    glVertex2i(panelX + panelW, panelY - panelH);
    glVertex2i(panelX, panelY - panelH);
    glEnd();

    glColor3f(0.25f, 0.35f, 0.45f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(panelX, panelY);
    glVertex2i(panelX + panelW, panelY);
    glVertex2i(panelX + panelW, panelY - panelH);
    glVertex2i(panelX, panelY - panelH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    renderBitmapString(panelX + 15, panelY - 20, GLUT_BITMAP_HELVETICA_12, "LIVE ZONE OCCUPANCY & DOCTOR FLEET STATUS");

    char buf[160];
    int startRowY = panelY - 42;
    int rowSpacing = 22;

    for (int z = 0; z < NUM_ZONES; z++) {
        int occ = getOccupiedCount((ZoneType)z);
        int tot = getTotalCount((ZoneType)z);
        int availDocs = getAvailableDoctorCount((ZoneType)z);

        float zr, zg, zb;
        getZoneColor((ZoneType)z, zr, zg, zb);
        glColor3f(zr, zg, zb);

        const char* shortNames[] = { "RED Critical", "YELLOW Urgent", "GREEN Minor",
                                     "BLUE Observe", "PURPLE Trauma", "WHITE Support", "ORANGE Overflow" };

        sprintf(buf, "[%-14s]  Beds: %d/%d (%s)  |  Docs Ready: %d",
                shortNames[z], occ, tot, (occ >= tot ? "FULL" : "Open"), availDocs);
        renderBitmapString(panelX + 15, startRowY - (z * rowSpacing), GLUT_BITMAP_HELVETICA_10, buf);
    }

    // Divider
    glColor3f(0.2f, 0.3f, 0.4f);
    glBegin(GL_LINES);
    glVertex2i(panelX + 15, startRowY - (NUM_ZONES * rowSpacing) + 6);
    glVertex2i(panelX + panelW - 15, startRowY - (NUM_ZONES * rowSpacing) + 6);
    glEnd();

    // Summary line
    int totalOccBeds = 0;
    int totalBeds = (int)beds.size();
    for (size_t i = 0; i < beds.size(); i++) if (beds[i].occupied) totalOccBeds++;
    float occPercent = ((float)totalOccBeds / (float)totalBeds) * 100.0f;

    glColor3f(0.9f, 0.95f, 1.0f);
    sprintf(buf, "Hospital Capacity: %d/%d (%.1f%%)  |  Treated: %d  |  Discharged: %d",
            totalOccBeds, totalBeds, occPercent, totalAdmitted, totalDischarged);
    renderBitmapString(panelX + 15, panelY - panelH + 42, GLUT_BITMAP_HELVETICA_10, buf);

    // Dynamic Surge Alert Badge
    if (surgeModeActive) {
        glColor3f(1.0f, 0.35f, 0.1f);
        renderBitmapString(panelX + 15, panelY - panelH + 18, GLUT_BITMAP_HELVETICA_10,
                           "DYNAMIC SURGE ACTIVE: Overflow Beds & Doctors Shifted to RED Zone!");
    } else {
        glColor3f(0.4f, 0.85f, 0.5f);
        renderBitmapString(panelX + 15, panelY - panelH + 18, GLUT_BITMAP_HELVETICA_10,
                           "System Status: Normal Operations (Standby Surge Allocation)");
    }

    // 3. BOTTOM NOTIFICATION / STATUS TICKER
    glColor4f(0.06f, 0.10f, 0.14f, 0.88f);
    glBegin(GL_QUADS);
    glVertex2i(0, 40);
    glVertex2i(windowWidth, 40);
    glVertex2i(windowWidth, 0);
    glVertex2i(0, 0);
    glEnd();

    glColor3f(0.2f, 0.6f, 0.9f);
    glBegin(GL_LINES);
    glVertex2i(0, 40);
    glVertex2i(windowWidth, 40);
    glEnd();

    glColor3f(1.0f, 0.9f, 0.2f);
    renderBitmapString(20, 16, GLUT_BITMAP_HELVETICA_12, "EVENT LOG: ");
    glColor3f(1.0f, 1.0f, 1.0f);
    renderBitmapString(115, 16, GLUT_BITMAP_HELVETICA_12, statusMessage.c_str());

    // 4. BOTTOM-LEFT INTERACTIVE CONTROLS GUIDE
    int ctrlX = 20;
    int ctrlY = 240;
    int ctrlW = 380;
    int ctrlH = 190;

    glColor4f(0.05f, 0.08f, 0.12f, 0.88f);
    glBegin(GL_QUADS);
    glVertex2i(ctrlX, ctrlY);
    glVertex2i(ctrlX + ctrlW, ctrlY);
    glVertex2i(ctrlX + ctrlW, ctrlY - ctrlH);
    glVertex2i(ctrlX, ctrlY - ctrlH);
    glEnd();

    glColor3f(0.25f, 0.35f, 0.45f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(ctrlX, ctrlY);
    glVertex2i(ctrlX + ctrlW, ctrlY);
    glVertex2i(ctrlX + ctrlW, ctrlY - ctrlH);
    glVertex2i(ctrlX, ctrlY - ctrlH);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    renderBitmapString(ctrlX + 15, ctrlY - 20, GLUT_BITMAP_HELVETICA_12, "INTERACTIVE KEYBOARD & CAMERA CONTROLS");

    glColor3f(0.75f, 0.85f, 0.95f);
    renderBitmapString(ctrlX + 15, ctrlY - 40, GLUT_BITMAP_HELVETICA_10, "[P]        : Admit Random Patient (Auto Triage)");
    renderBitmapString(ctrlX + 15, ctrlY - 58, GLUT_BITMAP_HELVETICA_10, "[1-3]      : Admit to Red, Yellow, Green Zones");
    renderBitmapString(ctrlX + 15, ctrlY - 76, GLUT_BITMAP_HELVETICA_10, "[4-7]      : Admit to Blue, Purple, White, Orange");
    renderBitmapString(ctrlX + 15, ctrlY - 94, GLUT_BITMAP_HELVETICA_10, "[D]        : Discharge Patient (Frees Bed)");
    renderBitmapString(ctrlX + 15, ctrlY - 112, GLUT_BITMAP_HELVETICA_10, "[C]        : Cycle 6 Camera Views (Overview, ICU, Bay...)");
    renderBitmapString(ctrlX + 15, ctrlY - 130, GLUT_BITMAP_HELVETICA_10, "[A]        : Dispatch Ambulance (Unloads Critical Patient)");
    renderBitmapString(ctrlX + 15, ctrlY - 148, GLUT_BITMAP_HELVETICA_10, "[O]        : Trigger Dynamic Surge Reallocation");
    renderBitmapString(ctrlX + 15, ctrlY - 166, GLUT_BITMAP_HELVETICA_10, "[L] / [R]  : Toggle Day/Night Light  |  [R] Reset Sim");
    renderBitmapString(ctrlX + 15, ctrlY - 182, GLUT_BITMAP_HELVETICA_10, "[Mouse Drag / Arrows]: Orbit & Zoom Camera");

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// ============================================================================
// CAMERA MANAGEMENT
// ============================================================================

void setupCamera() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float eyeX, eyeY, eyeZ;

    switch (currentView) {
        case VIEW_ISOMETRIC: {
            float radYaw = camYaw * (float)M_PI / 180.0f;
            float radPitch = camPitch * (float)M_PI / 180.0f;
            eyeX = camTargetX + camDist * cos(radPitch) * sin(radYaw);
            eyeY = camTargetY + camDist * sin(radPitch);
            eyeZ = camTargetZ + camDist * cos(radPitch) * cos(radYaw);
            gluLookAt(eyeX, eyeY, eyeZ, camTargetX, camTargetY, camTargetZ, 0.0f, 1.0f, 0.0f);
            break;
        }
        case VIEW_TOP_DOWN: {
            gluLookAt(0.0f, 65.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
            break;
        }
        case VIEW_RED_ZONE: {
            gluLookAt(-7.0f, 10.0f, 12.0f, -17.0f, 1.5f, 0.0f, 0.0f, 1.0f, 0.0f);
            break;
        }
        case VIEW_AMBULANCE_BAY: {
            gluLookAt(22.0f, 12.0f, 42.0f, 14.0f, 2.0f, 22.0f, 0.0f, 1.0f, 0.0f);
            break;
        }
        case VIEW_TRIAGE_DESK: {
            gluLookAt(4.0f, 7.0f, 26.0f, 4.0f, 1.5f, 6.0f, 0.0f, 1.0f, 0.0f);
            break;
        }
        case VIEW_SPECIALIZED_WING: {
            gluLookAt(0.0f, 12.0f, 6.0f, 0.0f, 1.5f, -15.0f, 0.0f, 1.0f, 0.0f);
            break;
        }
    }
}

// ============================================================================
// LIGHTING SETUP
// ============================================================================

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    if (nightMode) {
        glClearColor(0.04f, 0.05f, 0.08f, 1.0f);

        GLfloat ambientLight[] = { 0.15f, 0.18f, 0.22f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);

        GLfloat light0_pos[] = { -20.0f, 35.0f, 40.0f, 1.0f };
        GLfloat light0_diff[] = { 0.35f, 0.40f, 0.55f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diff);

        GLfloat light1_pos[] = { 0.0f, 18.0f, 0.0f, 1.0f };
        GLfloat light1_diff[] = { 0.85f, 0.90f, 0.95f, 1.0f };
        glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diff);
    } else {
        glClearColor(0.70f, 0.80f, 0.90f, 1.0f);

        GLfloat ambientLight[] = { 0.42f, 0.45f, 0.48f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);

        GLfloat light0_pos[] = { 30.0f, 45.0f, 35.0f, 1.0f };
        GLfloat light0_diff[] = { 0.95f, 0.95f, 0.90f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diff);

        GLfloat light1_pos[] = { -10.0f, 20.0f, -5.0f, 1.0f };
        GLfloat light1_diff[] = { 0.65f, 0.70f, 0.75f, 1.0f };
        glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diff);
    }
}

// ============================================================================
// MAIN DISPLAY CALLBACK
// ============================================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupLighting();
    setupCamera();

    // 1. Architecture, Floors, Walls & Zone Labels
    drawHospitalArchitecture();

    // 2. Triage Reception Station Desk & Nurse
    drawTriageStation(4.0f, 8.0f);

    // 3. Hospital Beds & Lying Patients
    for (size_t i = 0; i < beds.size(); i++) {
        drawHospitalBed(beds[i], heartBeatTimer);
    }

    // 4. Walking Patients (Moving from Entrance / Ambulance to Assigned Beds)
    for (size_t i = 0; i < walkingPatients.size(); i++) {
        if (!walkingPatients[i].arrived) {
            glPushMatrix();
            glTranslatef(walkingPatients[i].x, 0.0f, walkingPatients[i].z);
            glRotatef(walkingPatients[i].rotation, 0.0f, 1.0f, 0.0f);
            drawHuman(false, false, walkingPatients[i].legAngle, 0.0f,
                      walkingPatients[i].rColor, walkingPatients[i].gColor, walkingPatients[i].bColor);
            glPopMatrix();
        }
    }

    // 5. Doctors (Patrolling, Walking to Patient, or Performing Treatment)
    for (size_t i = 0; i < doctors.size(); i++) {
        glPushMatrix();
        glTranslatef(doctors[i].x, 0.0f, doctors[i].z);
        glRotatef(doctors[i].rotation, 0.0f, 1.0f, 0.0f);

        // White coat for doctor
        drawHuman(true, false, doctors[i].legAngle, doctors[i].treatAnimAngle, 0.95f, 0.95f, 0.98f);
        glPopMatrix();

        // If currently treating, draw animated medical examination indicator above bed
        if (doctors[i].state == DOC_TREATING && doctors[i].treatingBedIndex >= 0) {
            int bIdx = doctors[i].treatingBedIndex;
            drawTreatmentIndicator(beds[bIdx].x, 0.0f, beds[bIdx].z, doctors[i].treatmentTimer);
        }
    }

    // 6. Ambulance with Flashing Emergency Beacons
    drawAmbulance(ambX, ambZ, 180.0f, ambSirenActive, beaconState);

    // 7. 2D GUI Dashboard & HUD Overlay
    renderHUD();

    glutSwapBuffers();
}

// ============================================================================
// SIMULATION UPDATE & ANIMATION TIMER
// ============================================================================

void timer(int value) {
    simTimeMs += 16;
    heartBeatTimer += 0.12f;

    // Siren Beacon Toggle
    beaconTimer += 0.016f;
    if (beaconTimer >= 0.18f) {
        beaconTimer = 0.0f;
        beaconState = !beaconState;
    }

    // ------------------------------------------------------------------------
    // 1. AMBULANCE MOVEMENT & DISCHARGE INTEGRATION
    // ------------------------------------------------------------------------
    if (ambActive) {
        if (!ambArrived) {
            if (ambZ > ambTargetZ) {
                ambZ -= ambSpeed;
            } else {
                ambArrived = true;
                ambWaitTimer = 0.0f;
                ambDischargedPatient = false;
                statusMessage = "Ambulance docked at Bay! Offloading Critical Patient to Red Zone.";
            }
        } else {
            ambWaitTimer += 0.016f;

            // When docked, automatically discharge an emergency patient into the hospital
            if (ambWaitTimer > 0.8f && !ambDischargedPatient) {
                ambDischargedPatient = true;
                admitPatient(ZONE_RED, true); // true = spawns at ambulance rear doors
            }

            if (ambWaitTimer > 4.5f) {
                ambSirenActive = false;
            }

            if (ambWaitTimer > 8.0f) {
                // Return to road
                ambZ += ambSpeed;
                if (ambZ > 36.0f) {
                    ambActive = false;
                    ambArrived = false;
                    statusMessage = "Ambulance returned to patrol readiness.";
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // 2. PATIENT PATHFINDING & WALKING ANIMATION
    // ------------------------------------------------------------------------
    for (size_t i = 0; i < walkingPatients.size(); i++) {
        if (!walkingPatients[i].arrived) {
            WalkingPatient &p = walkingPatients[i];

            if (p.pathIndex < (int)p.path.size()) {
                Waypoint target = p.path[p.pathIndex];
                float dx = target.x - p.x;
                float dz = target.z - p.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.15f) {
                    p.x += (dx / dist) * p.speed;
                    p.z += (dz / dist) * p.speed;
                    p.rotation = atan2(dx, dz) * 180.0f / (float)M_PI;

                    // Leg walk cycle
                    if (p.legForward) {
                        p.legAngle += 4.5f;
                        if (p.legAngle > 28.0f) p.legForward = false;
                    } else {
                        p.legAngle -= 4.5f;
                        if (p.legAngle < -28.0f) p.legForward = true;
                    }
                } else {
                    p.pathIndex++;
                }
            } else {
                // Reached destination bed!
                p.arrived = true;
                int bIdx = p.targetBedIndex;
                beds[bIdx].occupied = true;
                beds[bIdx].patientId = p.id;

                char msg[128];
                sprintf(msg, "Patient #%d arrived and admitted to Bed #%d (%s Zone). Doctor notified.",
                        p.id, beds[bIdx].id, ZONE_NAMES[beds[bIdx].zone]);
                statusMessage = msg;

                // Dispatch doctor to treat this new patient
                dispatchDoctorToBed(bIdx);
            }
        }
    }

    // Clean up walking patients who have reached their beds
    for (int i = (int)walkingPatients.size() - 1; i >= 0; i--) {
        if (walkingPatients[i].arrived) {
            walkingPatients.erase(walkingPatients.begin() + i);
        }
    }

    // ------------------------------------------------------------------------
    // 3. DOCTOR INTERACTION, TREATMENT ANIMATION & PATROL
    // ------------------------------------------------------------------------
    for (size_t i = 0; i < doctors.size(); i++) {
        Doctor &doc = doctors[i];

        switch (doc.state) {
            case DOC_IDLE_PATROL: {
                // Subtle gentle patrolling near home zone
                float dx = doc.targetX - doc.x;
                float dz = doc.targetZ - doc.z;
                float dist = sqrt(dx * dx + dz * dz);

                if (dist > 0.2f) {
                    doc.x += (dx / dist) * doc.speed * 0.6f;
                    doc.z += (dz / dist) * doc.speed * 0.6f;
                    doc.rotation = atan2(dx, dz) * 180.0f / (float)M_PI;

                    if (doc.legForward) {
                        doc.legAngle += 2.5f;
                        if (doc.legAngle > 20.0f) doc.legForward = false;
                    } else {
                        doc.legAngle -= 2.5f;
                        if (doc.legAngle < -20.0f) doc.legForward = true;
                    }
                } else {
                    doc.legAngle = 0.0f;
                    // Pick subtle patrol waypoint near home
                    float jitterX = ((rand() % 100) / 100.0f - 0.5f) * 3.0f;
                    float jitterZ = ((rand() % 100) / 100.0f - 0.5f) * 3.0f;
                    doc.targetX = doc.homeX + jitterX;
                    doc.targetZ = doc.homeZ + jitterZ;
                }
                break;
            }

            case DOC_MOVING_TO_PATIENT: {
                if (doc.pathIndex < (int)doc.path.size()) {
                    Waypoint target = doc.path[doc.pathIndex];
                    float dx = target.x - doc.x;
                    float dz = target.z - doc.z;
                    float dist = sqrt(dx * dx + dz * dz);

                    if (dist > 0.15f) {
                        doc.x += (dx / dist) * doc.speed;
                        doc.z += (dz / dist) * doc.speed;
                        doc.rotation = atan2(dx, dz) * 180.0f / (float)M_PI;

                        if (doc.legForward) {
                            doc.legAngle += 4.5f;
                            if (doc.legAngle > 26.0f) doc.legForward = false;
                        } else {
                            doc.legAngle -= 4.5f;
                            if (doc.legAngle < -26.0f) doc.legForward = true;
                        }
                    } else {
                        doc.pathIndex++;
                    }
                } else {
                    // Reached patient bedside -> Begin Treatment
                    doc.state = DOC_TREATING;
                    doc.treatmentTimer = 0.0f;
                    doc.legAngle = 0.0f;
                    doc.treatAnimAngle = 0.0f;

                    char msg[128];
                    sprintf(msg, "Doctor #%d commenced medical treatment on Bed #%d.",
                            doc.id, beds[doc.treatingBedIndex].id);
                    statusMessage = msg;
                }
                break;
            }

            case DOC_TREATING: {
                doc.treatmentTimer += 0.016f;
                // Animate arm examination movement
                doc.treatAnimAngle = 25.0f * sin(doc.treatmentTimer * 4.0f);

                // Treatment duration ~ 3.5 seconds
                if (doc.treatmentTimer > 3.5f) {
                    char msg[128];
                    sprintf(msg, "Doctor #%d completed treatment on Bed #%d. Patient vitals stabilized.",
                            doc.id, beds[doc.treatingBedIndex].id);
                    statusMessage = msg;

                    // Return back to home station
                    doc.state = DOC_RETURNING;
                    doc.treatAnimAngle = 0.0f;
                    doc.path = buildCorridorPath(doc.x, doc.z, doc.homeX, doc.homeZ);
                    doc.pathIndex = 0;
                }
                break;
            }

            case DOC_RETURNING: {
                if (doc.pathIndex < (int)doc.path.size()) {
                    Waypoint target = doc.path[doc.pathIndex];
                    float dx = target.x - doc.x;
                    float dz = target.z - doc.z;
                    float dist = sqrt(dx * dx + dz * dz);

                    if (dist > 0.15f) {
                        doc.x += (dx / dist) * doc.speed;
                        doc.z += (dz / dist) * doc.speed;
                        doc.rotation = atan2(dx, dz) * 180.0f / (float)M_PI;

                        if (doc.legForward) {
                            doc.legAngle += 4.0f;
                            if (doc.legAngle > 24.0f) doc.legForward = false;
                        } else {
                            doc.legAngle -= 4.0f;
                            if (doc.legAngle < -24.0f) doc.legForward = true;
                        }
                    } else {
                        doc.pathIndex++;
                    }
                } else {
                    // Back home
                    doc.state = DOC_IDLE_PATROL;
                    doc.legAngle = 0.0f;
                    doc.targetX = doc.homeX;
                    doc.targetZ = doc.homeZ;
                }
                break;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// ============================================================================
// KEYBOARD CONTROLS
// ============================================================================

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;

        case 'p':
        case 'P': {
            // Automatic Triage sorting by realistic clinical priority:
            // 20% Red, 20% Yellow, 20% Green, 15% Blue, 10% Purple, 10% White, 5% Orange
            int r = rand() % 100;
            if (r < 20)       admitPatient(ZONE_RED);
            else if (r < 40)  admitPatient(ZONE_YELLOW);
            else if (r < 60)  admitPatient(ZONE_GREEN);
            else if (r < 75)  admitPatient(ZONE_BLUE);
            else if (r < 85)  admitPatient(ZONE_PURPLE);
            else if (r < 95)  admitPatient(ZONE_WHITE);
            else              admitPatient(ZONE_ORANGE);
            break;
        }

        // Direct Zone Admissions
        case '1': admitPatient(ZONE_RED);    break;
        case '2': admitPatient(ZONE_YELLOW); break;
        case '3': admitPatient(ZONE_GREEN);  break;
        case '4': admitPatient(ZONE_BLUE);   break;
        case '5': admitPatient(ZONE_PURPLE); break;
        case '6': admitPatient(ZONE_WHITE);  break;
        case '7': admitPatient(ZONE_ORANGE); break;

        case 'd':
        case 'D':
            dischargePatient();
            break;

        case 'c':
        case 'C': {
            currentView = (CameraView)((currentView + 1) % 6);
            const char* viewNames[] = {
                "Isometric Overview",
                "Top-Down Floor Plan",
                "Red Zone (Critical Care)",
                "Ambulance Drop-Off Bay",
                "Triage Reception Desk",
                "Specialized / Trauma Wing"
            };
            char msg[128];
            sprintf(msg, "Switched Camera View to: %s", viewNames[currentView]);
            statusMessage = msg;
            break;
        }

        case 'a':
        case 'A':
            // Dispatch ambulance run with full emergency sequence
            ambActive = true;
            ambZ = 34.0f;
            ambArrived = false;
            ambSirenActive = true;
            ambDischargedPatient = false;
            statusMessage = "EMERGENCY: Ambulance dispatched! In-transit with critical patient.";
            break;

        case 'o':
        case 'O':
            // Trigger Dynamic Surge / Resource Allocation
            checkAndReallocateResources();
            break;

        case 'l':
        case 'L':
            nightMode = !nightMode;
            statusMessage = nightMode ? "Switched to NIGHT / EMERGENCY Lighting Mode." : "Switched to DAYLIGHT Mode.";
            break;

        case 'r':
        case 'R':
            initHospitalLayout();
            statusMessage = "Simulator reset to default state.";
            break;
    }

    glutPostRedisplay();
}

// Special Keys: Navigation in Isometric mode
void specialKeys(int key, int x, int y) {
    if (currentView != VIEW_ISOMETRIC) {
        currentView = VIEW_ISOMETRIC;
    }

    switch (key) {
        case GLUT_KEY_LEFT:      camYaw -= 4.0f; break;
        case GLUT_KEY_RIGHT:     camYaw += 4.0f; break;
        case GLUT_KEY_UP:        camPitch += 3.0f; if (camPitch > 85.0f) camPitch = 85.0f; break;
        case GLUT_KEY_DOWN:      camPitch -= 3.0f; if (camPitch < 10.0f) camPitch = 10.0f; break;
        case GLUT_KEY_PAGE_UP:   camDist -= 2.0f; if (camDist < 15.0f) camDist = 15.0f; break;
        case GLUT_KEY_PAGE_DOWN: camDist += 2.0f; if (camDist > 90.0f) camDist = 90.0f; break;
    }
    glutPostRedisplay();
}

// Mouse Controls
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else if (state == GLUT_UP) {
            isDragging = false;
        }
    } else if (button == 3) {
        camDist -= 2.0f;
        if (camDist < 15.0f) camDist = 15.0f;
        glutPostRedisplay();
    } else if (button == 4) {
        camDist += 2.0f;
        if (camDist > 90.0f) camDist = 90.0f;
        glutPostRedisplay();
    }
}

void motion(int x, int y) {
    if (isDragging) {
        float dx = (float)(x - lastMouseX);
        float dy = (float)(y - lastMouseY);

        camYaw += dx * 0.4f;
        camPitch += dy * 0.4f;

        if (camPitch > 85.0f) camPitch = 85.0f;
        if (camPitch < 5.0f)  camPitch = 5.0f;

        lastMouseX = x;
        lastMouseY = y;

        if (currentView != VIEW_ISOMETRIC) {
            currentView = VIEW_ISOMETRIC;
        }

        glutPostRedisplay();
    }
}

// Window Reshape Callback
void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = (h == 0) ? 1 : h;

    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)windowWidth / (double)windowHeight, 1.0, 300.0);

    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// MAIN FUNCTION & OPENGL INITIALIZATION
// ============================================================================

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("3D Dynamic Crowd Emergency Management & Resource Allocation Simulator");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    quadric = gluNewQuadric();
    gluQuadricNormals(quadric, GLU_SMOOTH);

    initHospitalLayout();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(16, timer, 0);

    printf("======================================================================\n");
    printf(" 3D Dynamic Crowd Emergency Management and Resource Allocation Simulator\n");
    printf("======================================================================\n");
    printf(" Controls:\n");
    printf("  [P]         : Admit Patient (Automatic Triage Sorting)\n");
    printf("  [1 to 7]    : Admit Patient to Red, Yellow, Green, Blue, Purple, White, Orange\n");
    printf("  [D]         : Discharge Treated Patient (Free Bed)\n");
    printf("  [C]         : Cycle Camera Views (Overview, Top-Down, Red, Bay, Triage, Trauma)\n");
    printf("  [A]         : Dispatch Ambulance (Arrival, Beacon Sirens & Walk-in Patient)\n");
    printf("  [O]         : Trigger Dynamic Surge Reallocation\n");
    printf("  [L]         : Toggle Day / Night Emergency Lighting\n");
    printf("  [R]         : Reset Simulator to Initial State\n");
    printf("  [Mouse Drag]: Orbit Camera around the 3D Hospital\n");
    printf("  [ESC]       : Exit\n");
    printf("======================================================================\n");

    glutMainLoop();

    gluDeleteQuadric(quadric);
    return 0;
}
