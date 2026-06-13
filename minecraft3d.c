// minecraft3d.c - Minecraft-style block engine with ray tracer
// Compile: gcc -o minecraft3d.exe minecraft3d.c -lopengl32 -lglu32 -lm -mwindows

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// BLOCK DATA
// ============================================================================
#define WORLD_WIDTH 32
#define WORLD_HEIGHT 16
#define WORLD_DEPTH 32

typedef struct {
    unsigned char type; // 0=air, 1=grass, 2=dirt, 3=stone, 4=wood, 5=leaf
    unsigned char light;
} Block;

Block g_world[WORLD_WIDTH][WORLD_HEIGHT][WORLD_DEPTH];

// ============================================================================
// MATH
// ============================================================================
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y; } Vec2;

Vec3 vec3(float x, float y, float z) { Vec3 v = {x, y, z}; return v; }
Vec3 add(Vec3 a, Vec3 b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
Vec3 sub(Vec3 a, Vec3 b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
Vec3 mul(Vec3 v, float s) { return vec3(v.x*s, v.y*s, v.z*s); }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
Vec3 normalize(Vec3 v) { float len = sqrt(dot(v,v)); if(len<0.0001) return vec3(0,0,0); return mul(v, 1.0f/len); }

// ============================================================================
// GLOBALS
// ============================================================================
HWND g_hwnd;
HDC g_hdc;
HGLRC g_hrc;
int g_width = 1024;
int g_height = 768;

// Camera (FIXED CONTROLS)
float g_camX = 16, g_camY = 5, g_camZ = 16;
float g_camAngleX = 0, g_camAngleY = 0;
int g_keys[256] = {0};
int g_mouseX = 512, g_mouseY = 384;
int g_firstMouse = 1;

// Ray tracing mode
int g_rendering = 0;
float g_renderProgress = 0;
int g_renderWidth = 1280;
int g_renderHeight = 720;

// Block textures (simulated colors)
Vec3 g_blockColors[6] = {
    {0.0, 0.0, 0.0},       // 0: air
    {0.4, 0.8, 0.2},       // 1: grass (top)
    {0.6, 0.4, 0.2},       // 2: dirt
    {0.5, 0.5, 0.5},       // 3: stone
    {0.6, 0.4, 0.1},       // 4: wood
    {0.2, 0.6, 0.1}        // 5: leaf
};

// ============================================================================
// WORLD GENERATION
// ============================================================================
void generateWorld() {
    // Clear world
    for(int x = 0; x < WORLD_WIDTH; x++)
        for(int y = 0; y < WORLD_HEIGHT; y++)
            for(int z = 0; z < WORLD_DEPTH; z++)
                g_world[x][y][z].type = 0;
    
    // Generate terrain using simplex-like noise (simple version)
    srand(42); // Seed for consistent world
    
    for(int x = 0; x < WORLD_WIDTH; x++) {
        for(int z = 0; z < WORLD_DEPTH; z++) {
            // Heightmap using sine/cosine for hills
            float height = 4.0f;
            height += sin(x * 0.3f) * 1.5f;
            height += cos(z * 0.3f) * 1.5f;
            height += sin((x+z) * 0.5f) * 1.0f;
            height = fmax(2, fmin(10, height));
            
            int groundY = (int)height;
            
            for(int y = 0; y <= groundY; y++) {
                if(y == groundY) {
                    g_world[x][y][z].type = 1; // Grass top
                } else if(y > groundY - 3) {
                    g_world[x][y][z].type = 2; // Dirt
                } else {
                    g_world[x][y][z].type = 3; // Stone
                }
            }
        }
    }
    
    // Add some trees
    for(int t = 0; t < 30; t++) {
        int x = 5 + rand() % (WORLD_WIDTH - 10);
        int z = 5 + rand() % (WORLD_DEPTH - 10);
        int groundY = 0;
        
        // Find ground height
        for(int y = WORLD_HEIGHT-1; y >= 0; y--) {
            if(g_world[x][y][z].type != 0) {
                groundY = y + 1;
                break;
            }
        }
        
        // Tree trunk
        if(groundY > 0 && groundY < WORLD_HEIGHT-5) {
            for(int y = 0; y < 4; y++) {
                if(groundY + y < WORLD_HEIGHT) {
                    g_world[x][groundY + y][z].type = 4; // Wood
                }
            }
            
            // Leaves
            for(int ly = -1; ly <= 2; ly++) {
                for(int lx = -2; lx <= 2; lx++) {
                    for(int lz = -2; lz <= 2; lz++) {
                        int dist = abs(lx) + abs(lz) + abs(ly);
                        if(dist <= 3 && groundY + 3 + ly < WORLD_HEIGHT) {
                            int nx = x + lx;
                            int nz = z + lz;
                            if(nx >= 0 && nx < WORLD_WIDTH && nz >= 0 && nz < WORLD_DEPTH) {
                                if(g_world[nx][groundY + 3 + ly][nz].type == 0) {
                                    g_world[nx][groundY + 3 + ly][nz].type = 5; // Leaf
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Calculate simple lighting (fake shadows)
    for(int x = 0; x < WORLD_WIDTH; x++) {
        for(int z = 0; z < WORLD_DEPTH; z++) {
            int light = 15;
            for(int y = WORLD_HEIGHT-1; y >= 0; y--) {
                if(g_world[x][y][z].type != 0) {
                    g_world[x][y][z].light = light;
                    light = (light > 3) ? light - 3 : 0;
                } else {
                    g_world[x][y][z].light = light;
                }
            }
        }
    }
}

// ============================================================================
// BLOCK RENDERING (Real-time preview)
// ============================================================================
void drawBlock(float x, float y, float z, int type, int light) {
    Vec3 color = g_blockColors[type];
    float brightness = 0.3f + (light / 15.0f) * 0.7f;
    
    glColor3f(color.x * brightness, color.y * brightness, color.z * brightness);
    
    glBegin(GL_QUADS);
        // Front
        glVertex3f(x, y, z+1);
        glVertex3f(x+1, y, z+1);
        glVertex3f(x+1, y+1, z+1);
        glVertex3f(x, y+1, z+1);
        
        // Back
        glVertex3f(x, y, z);
        glVertex3f(x, y+1, z);
        glVertex3f(x+1, y+1, z);
        glVertex3f(x+1, y, z);
        
        // Left
        glVertex3f(x, y, z);
        glVertex3f(x, y, z+1);
        glVertex3f(x, y+1, z+1);
        glVertex3f(x, y+1, z);
        
        // Right
        glVertex3f(x+1, y, z);
        glVertex3f(x+1, y+1, z);
        glVertex3f(x+1, y+1, z+1);
        glVertex3f(x+1, y, z+1);
        
        // Top (grass has special color)
        if(type == 1) {
            glColor3f(0.3f * brightness, 0.7f * brightness, 0.1f * brightness);
        }
        glVertex3f(x, y+1, z);
        glVertex3f(x, y+1, z+1);
        glVertex3f(x+1, y+1, z+1);
        glVertex3f(x+1, y+1, z);
        
        // Bottom
        if(type == 1) glColor3f(0.5f * brightness, 0.3f * brightness, 0.1f * brightness);
        glVertex3f(x, y, z);
        glVertex3f(x+1, y, z);
        glVertex3f(x+1, y, z+1);
        glVertex3f(x, y, z+1);
    glEnd();
}

void renderWorld() {
    // Only render blocks within view distance
    int viewDist = 12;
    
    for(int x = (int)(g_camX - viewDist); x <= (int)(g_camX + viewDist); x++) {
        if(x < 0 || x >= WORLD_WIDTH) continue;
        
        for(int z = (int)(g_camZ - viewDist); z <= (int)(g_camZ + viewDist); z++) {
            if(z < 0 || z >= WORLD_DEPTH) continue;
            
            for(int y = 0; y < WORLD_HEIGHT; y++) {
                if(g_world[x][y][z].type != 0) {
                    // Frustum culling
                    float dx = x - g_camX;
                    float dz = z - g_camZ;
                    float dy = y - g_camY;
                    float dist = sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if(dist < viewDist + 2) {
                        drawBlock((float)x, (float)y, (float)z, 
                                  g_world[x][y][z].type, 
                                  g_world[x][y][z].light);
                    }
                }
            }
        }
    }
}

// ============================================================================
// RAY TRACED RENDER (Fake shadows Minecraft style)
// ============================================================================
typedef struct { Vec3 point, normal; int type; float t; int hit; } Hit;

Hit raycastBlock(Vec3 origin, Vec3 direction) {
    Hit hit = {0};
    float bestT = 1000;
    
    // Digital Differential Analyzer (DDA) for voxel traversal
    for(int x = 0; x < WORLD_WIDTH; x++) {
        for(int z = 0; z < WORLD_DEPTH; z++) {
            for(int y = 0; y < WORLD_HEIGHT; y++) {
                if(g_world[x][y][z].type != 0) {
                    // AABB intersection
                    Vec3 boxMin = vec3(x, y, z);
                    Vec3 boxMax = vec3(x+1, y+1, z+1);
                    
                    float t1 = (boxMin.x - origin.x) / direction.x;
                    float t2 = (boxMax.x - origin.x) / direction.x;
                    float t3 = (boxMin.y - origin.y) / direction.y;
                    float t4 = (boxMax.y - origin.y) / direction.y;
                    float t5 = (boxMin.z - origin.z) / direction.z;
                    float t6 = (boxMax.z - origin.z) / direction.z;
                    
                    float tmin = fmax(fmax(fmin(t1, t2), fmin(t3, t4)), fmin(t5, t6));
                    float tmax = fmin(fmin(fmax(t1, t2), fmax(t3, t4)), fmax(t5, t6));
                    
                    if(tmin <= tmax && tmax > 0.001 && tmin < bestT) {
                        bestT = tmin;
                        hit.hit = 1;
                        hit.t = tmin;
                        hit.type = g_world[x][y][z].type;
                        hit.point = add(origin, mul(direction, tmin));
                        
                        // Calculate normal
                        if(fabs(hit.point.x - x) < 0.01) hit.normal = vec3(-1,0,0);
                        else if(fabs(hit.point.x - (x+1)) < 0.01) hit.normal = vec3(1,0,0);
                        else if(fabs(hit.point.y - y) < 0.01) hit.normal = vec3(0,-1,0);
                        else if(fabs(hit.point.y - (y+1)) < 0.01) hit.normal = vec3(0,1,0);
                        else if(fabs(hit.point.z - z) < 0.01) hit.normal = vec3(0,0,-1);
                        else hit.normal = vec3(0,0,1);
                    }
                }
            }
        }
    }
    
    return hit;
}

Vec3 traceBlockRay(Vec3 origin, Vec3 direction, int depth) {
    if(depth > 3) return vec3(0,0,0);
    
    Hit hit = raycastBlock(origin, direction);
    
    if(!hit.hit) {
        // Sky
        return vec3(0.5, 0.6, 0.8);
    }
    
    Vec3 color = g_blockColors[hit.type];
    
    // Fake Minecraft-style shadows
    float shadow = 0.6f;
    
    // Ambient occlusion based on neighbors
    int x = (int)hit.point.x;
    int y = (int)hit.point.y;
    int z = (int)hit.point.z;
    
    if(hit.normal.y > 0) {
        // Top face - brightest
        shadow = 0.9f;
    } else if(hit.normal.y < 0) {
        // Bottom face - darkest
        shadow = 0.4f;
    } else {
        // Side faces
        shadow = 0.7f;
        
        // Simple ambient occlusion
        int nx = x + (int)hit.normal.x;
        int nz = z + (int)hit.normal.z;
        if(nx >= 0 && nx < WORLD_WIDTH && nz >= 0 && nz < WORLD_DEPTH) {
            if(y < WORLD_HEIGHT-1 && g_world[nx][y+1][nz].type != 0) shadow *= 0.7f;
            if(y > 0 && g_world[nx][y-1][nz].type != 0) shadow *= 0.8f;
        }
    }
    
    // Sun light (directional)
    Vec3 sunDir = normalize(vec3(0.5, 1, 0.3));
    float sunlight = fmax(0.2, dot(hit.normal, sunDir));
    shadow *= (0.4f + sunlight * 0.6f);
    
    // Sky light from above
    int lightY = y+1;
    while(lightY < WORLD_HEIGHT && g_world[x][lightY][z].type == 0) lightY++;
    float skylight = 1.0f - (lightY - y) / 20.0f;
    if(skylight < 0.2f) skylight = 0.2f;
    
    float brightness = shadow * skylight;
    if(brightness < 0.15f) brightness = 0.15f;
    
    // Special grass top color
    if(hit.type == 1 && hit.normal.y > 0) {
        color = vec3(0.4f, 0.85f, 0.25f);
    }
    
    return mul(color, brightness);
}

void renderHighQuality() {
    g_rendering = 1;
    
    unsigned char* pixels = (unsigned char*)malloc(g_renderWidth * g_renderHeight * 3);
    
    Vec3 camPos = vec3(g_camX, g_camY, g_camZ);
    Vec3 camDir = normalize(vec3(sin(g_camAngleX), sin(g_camAngleY), cos(g_camAngleX)));
    Vec3 camRight = normalize(cross(camDir, vec3(0,1,0)));
    Vec3 camUp = normalize(cross(camRight, camDir));
    
    float fov = 70.0f * 3.14159f / 180.0f;
    float aspect = (float)g_renderWidth / g_renderHeight;
    
    for(int y = 0; y < g_renderHeight; y++) {
        if(y % 50 == 0) {
            g_renderProgress = (float)y / g_renderHeight;
            InvalidateRect(g_hwnd, NULL, FALSE);
            MSG msg;
            while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        for(int x = 0; x < g_renderWidth; x++) {
            float u = (2.0f * x / g_renderWidth - 1.0f) * tan(fov/2) * aspect;
            float v = (1.0f - 2.0f * y / g_renderHeight) * tan(fov/2);
            
            Vec3 rayDir = normalize(add(add(camDir, mul(camRight, u)), mul(camUp, v)));
            Vec3 color = traceBlockRay(camPos, rayDir, 0);
            
            // Gamma
            color.x = pow(color.x, 1.0f/2.2f);
            color.y = pow(color.y, 1.0f/2.2f);
            color.z = pow(color.z, 1.0f/2.2f);
            
            if(color.x > 1) color.x = 1;
            if(color.y > 1) color.y = 1;
            if(color.z > 1) color.z = 1;
            
            pixels[(y * g_renderWidth + x) * 3 + 0] = (unsigned char)(color.x * 255);
            pixels[(y * g_renderWidth + x) * 3 + 1] = (unsigned char)(color.y * 255);
            pixels[(y * g_renderWidth + x) * 3 + 2] = (unsigned char)(color.z * 255);
        }
    }
    
    // Save BMP
    BITMAPFILEHEADER bf = {0};
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = g_renderWidth;
    bi.biHeight = g_renderHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    bf.bfType = 0x4D42;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bf.bfSize = bf.bfOffBits + ((g_renderWidth * 3 + 3) & ~3) * g_renderHeight;
    
    char fileName[MAX_PATH] = "minecraft_render.bmp";
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "Bitmap Files\0*.bmp\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if(GetSaveFileName(&ofn)) {
        HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, &bf, sizeof(bf), &written, NULL);
            WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
            
            for(int y = g_renderHeight-1; y >= 0; y--) {
                WriteFile(hFile, pixels + y * g_renderWidth * 3, g_renderWidth * 3, &written, NULL);
                int padding = (4 - (g_renderWidth * 3) % 4) % 4;
                if(padding) {
                    char pad[4] = {0};
                    WriteFile(hFile, pad, padding, &written, NULL);
                }
            }
            CloseHandle(hFile);
            MessageBox(g_hwnd, "Minecraft render complete!", "3DSoft Minecraft", MB_OK);
        }
    }
    
    free(pixels);
    g_rendering = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

// ============================================================================
// RENDERING PREVIEW
// ============================================================================
void renderPreview() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Camera - FIXED: W=forward, S=backward
    gluLookAt(g_camX, g_camY, g_camZ,
              g_camX + sin(g_camAngleX), g_camY + sin(g_camAngleY), g_camZ + cos(g_camAngleX),
              0, 1, 0);
    
    renderWorld();
    
    if(g_rendering) {
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, g_width, 0, g_height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        glBegin(GL_QUADS);
        glColor3f(0,0,0);
        glVertex2f(g_width/2 - 300, g_height - 80);
        glVertex2f(g_width/2 + 300, g_height - 80);
        glVertex2f(g_width/2 + 300, g_height - 60);
        glVertex2f(g_width/2 - 300, g_height - 60);
        
        glColor3f(0.4f, 0.7f, 0.2f);
        glVertex2f(g_width/2 - 298, g_height - 78);
        glVertex2f(g_width/2 - 298 + 596 * g_renderProgress, g_height - 78);
        glVertex2f(g_width/2 - 298 + 596 * g_renderProgress, g_height - 62);
        glVertex2f(g_width/2 - 298, g_height - 62);
        glEnd();
        
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
    }
    
    SwapBuffers(g_hdc);
}

// ============================================================================
// INPUT HANDLING (FIXED: W=forward, S=backward)
// ============================================================================
void handleInput() {
    if(g_rendering) return;
    
    float speed = 0.1f;
    float forwardX = sin(g_camAngleX);
    float forwardZ = cos(g_camAngleX);
    float rightX = cos(g_camAngleX);
    float rightZ = -sin(g_camAngleX);
    
    // FIXED CONTROLS: W = forward, S = backward
    if(g_keys['W'] || g_keys['w']) {
        g_camX += forwardX * speed;
        g_camZ += forwardZ * speed;
    }
    if(g_keys['S'] || g_keys['s']) {
        g_camX -= forwardX * speed;
        g_camZ -= forwardZ * speed;
    }
    if(g_keys['A'] || g_keys['a']) {
        g_camX -= rightX * speed;
        g_camZ -= rightZ * speed;
    }
    if(g_keys['D'] || g_keys['d']) {
        g_camX += rightX * speed;
        g_camZ += rightZ * speed;
    }
    if(g_keys[VK_SPACE]) g_camY += speed;
    if(g_keys[VK_SHIFT]) g_camY -= speed;
    
    // Clamp camera position
    if(g_camX < 1) g_camX = 1;
    if(g_camX > WORLD_WIDTH-2) g_camX = WORLD_WIDTH-2;
    if(g_camZ < 1) g_camZ = 1;
    if(g_camZ > WORLD_DEPTH-2) g_camZ = WORLD_DEPTH-2;
    if(g_camY < 1) g_camY = 1;
    if(g_camY > WORLD_HEIGHT-2) g_camY = WORLD_HEIGHT-2;
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            g_keys[wParam] = 1;
            if(wParam == VK_ESCAPE) PostQuitMessage(0);
            if((wParam == 'R' || wParam == 'r') && !g_rendering) {
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)renderHighQuality, NULL, 0, NULL);
            }
            return 0;
        case WM_KEYUP:
            g_keys[wParam] = 0;
            return 0;
        case WM_MOUSEMOVE:
            if(!g_rendering) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                if(g_firstMouse) {
                    g_mouseX = x; g_mouseY = y;
                    g_firstMouse = 0;
                }
                int dx = x - g_mouseX;
                int dy = y - g_mouseY;
                g_camAngleX += dx * 0.005f;
                g_camAngleY -= dy * 0.003f;
                if(g_camAngleY > 1.4f) g_camAngleY = 1.4f;
                if(g_camAngleY < -1.2f) g_camAngleY = -1.2f;
                g_mouseX = x; g_mouseY = y;
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                if(x <= 10 || x >= rect.right-10 || y <= 10 || y >= rect.bottom-10) {
                    SetCursorPos(rect.right/2, rect.bottom/2);
                    g_mouseX = rect.right/2;
                    g_mouseY = rect.bottom/2;
                }
            }
            return 0;
        case WM_SIZE:
            g_width = LOWORD(lParam);
            g_height = HIWORD(lParam);
            glViewport(0, 0, g_width, g_height);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(70.0, (double)g_width/g_height, 0.1, 100.0);
            glMatrixMode(GL_MODELVIEW);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MAIN
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Minecraft3D";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    
    g_hwnd = CreateWindowEx(0, "Minecraft3D", 
                            "MINECRAFT 3D | WASD to move | Mouse to look | R to render high quality",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, g_width, g_height,
                            NULL, NULL, hInstance, NULL);
    
    if(!g_hwnd) return 1;
    
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,PFD_MAIN_PLANE,0,0,0,0
    };
    SetPixelFormat(g_hdc, ChoosePixelFormat(g_hdc, &pfd), &pfd);
    g_hrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_hrc);
    
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.5f, 0.6f, 0.8f, 1.0f);
    
    generateWorld();
    
    // Center mouse
    RECT rect;
    GetClientRect(g_hwnd, &rect);
    SetCursorPos(rect.right/2, rect.bottom/2);
    ShowCursor(FALSE);
    
    MSG msg;
    while(1) {
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(g_hrc);
                ReleaseDC(g_hwnd, g_hdc);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        handleInput();
        renderPreview();
        Sleep(16);
    }
    
    return 0;
}
