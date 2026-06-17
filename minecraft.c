// ============================================================
//  MINECRAFT EARLY DAYS - Raylib Version (FIXED)
//  Compile: gcc -o minecraft minecraft.c -lraylib -lm
// ============================================================

#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
//  CONSTANTS
// ============================================================
#define WORLD_SIZE 32
#define BLOCK_SIZE 1.0f
#define GRAVITY -25.0f
#define JUMP_SPEED 8.0f
#define PLAYER_SPEED 4.5f
#define PLAYER_HEIGHT 1.62f
#define MOUSE_SENSITIVITY 0.002f

// ============================================================
//  WORLD DATA
// ============================================================
int world[WORLD_SIZE][WORLD_SIZE];

// ============================================================
//  PLAYER STRUCT
// ============================================================
typedef struct {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    bool onGround;
    bool cursorLocked;
} Player;

Player player = {
    .position = {WORLD_SIZE/2.0f, 0, WORLD_SIZE/2.0f},
    .velocity = {0, 0, 0},
    .yaw = 0,
    .pitch = 0,
    .onGround = false,
    .cursorLocked = false
};

// ============================================================
//  TEXTURE
// ============================================================
Texture2D grassTexture;
Material blockMaterial;

// ============================================================
//  FUNCTIONS
// ============================================================
float getHeight(int x, int z) {
    float h = 0;
    h += sinf(x * 0.25f + z * 0.15f) * 0.8f;
    h += cosf(z * 0.2f - x * 0.1f) * 0.6f;
    h += sinf((x + z) * 0.15f) * 0.5f;
    h += cosf(x * 0.4f + z * 0.6f) * 0.3f;
    
    // Flatten edges
    int edge = 4;
    if (x < edge) h -= (edge - x) * 0.5f;
    if (x > WORLD_SIZE - edge) h -= (x - (WORLD_SIZE - edge)) * 0.5f;
    if (z < edge) h -= (edge - z) * 0.5f;
    if (z > WORLD_SIZE - edge) h -= (z - (WORLD_SIZE - edge)) * 0.5f;
    
    return h > 0 ? h + 2 : 2;
}

void generateWorld() {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            world[x][z] = (int)roundf(getHeight(x, z));
        }
    }
}

int getBlockAt(int x, int y, int z) {
    if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE) return -1;
    if (y < 0) return -1;
    if (y <= world[x][z]) return 1;
    return -1;
}

bool isSolid(int x, int y, int z) {
    return getBlockAt(x, y, z) > 0;
}

void drawBlock(int x, int y, int z) {
    Vector3 pos = {x + 0.5f, y + 0.5f, z + 0.5f};
    
    // Draw cube with material (texture)
    DrawCubeV(pos, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
    
    // Draw wireframe outline (very light)
    DrawCubeWiresV(pos, (Vector3){1.0f, 1.0f, 1.0f}, (Color){0, 0, 0, 30});
}

void drawWorld() {
    int renderDist = 12;
    int px = (int)floorf(player.position.x);
    int pz = (int)floorf(player.position.z);
    
    for (int x = px - renderDist; x < px + renderDist; x++) {
        for (int z = pz - renderDist; z < pz + renderDist; z++) {
            if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE) continue;
            
            int height = world[x][z];
            for (int y = 0; y <= height; y++) {
                // Check if block is visible (not fully surrounded)
                bool visible = false;
                
                // Check neighbors
                if (!isSolid(x-1, y, z)) visible = true;
                if (!isSolid(x+1, y, z)) visible = true;
                if (!isSolid(x, y-1, z)) visible = true;
                if (!isSolid(x, y+1, z)) visible = true;
                if (!isSolid(x, y, z-1)) visible = true;
                if (!isSolid(x, y, z+1)) visible = true;
                
                if (visible) {
                    drawBlock(x, y, z);
                }
            }
        }
    }
}

void updatePlayer(float dt) {
    // Mouse look
    if (player.cursorLocked) {
        Vector2 mouseDelta = GetMouseDelta();
        player.yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
        player.pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
        player.pitch = fmaxf(-1.5f, fminf(1.5f, player.pitch));
    }
    
    // Movement
    Vector3 forward = { -sinf(player.yaw), 0, -cosf(player.yaw) };
    Vector3 right = { cosf(player.yaw), 0, -sinf(player.yaw) };
    
    Vector3 moveDir = {0, 0, 0};
    if (IsKeyDown(KEY_W)) { moveDir.x += forward.x; moveDir.z += forward.z; }
    if (IsKeyDown(KEY_S)) { moveDir.x -= forward.x; moveDir.z -= forward.z; }
    if (IsKeyDown(KEY_A)) { moveDir.x -= right.x; moveDir.z -= right.z; }
    if (IsKeyDown(KEY_D)) { moveDir.x += right.x; moveDir.z += right.z; }
    
    if (moveDir.x != 0 || moveDir.z != 0) {
        float len = sqrtf(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
        moveDir.x /= len;
        moveDir.z /= len;
    }
    
    // Sprint
    float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 6.0f : PLAYER_SPEED;
    moveDir.x *= speed * dt;
    moveDir.z *= speed * dt;
    
    // Jump
    if (IsKeyPressed(KEY_SPACE) && player.onGround) {
        player.velocity.y = JUMP_SPEED;
        player.onGround = false;
    }
    
    // Gravity
    player.velocity.y += GRAVITY * dt;
    if (player.velocity.y < -20.0f) player.velocity.y = -20.0f;
    
    // --- Collision Detection ---
    // X movement
    float newX = player.position.x + moveDir.x;
    int bx = (int)floorf(newX);
    int bz = (int)floorf(player.position.z);
    int by = (int)floorf(player.position.y);
    int headY = (int)floorf(player.position.y + PLAYER_HEIGHT);
    
    bool canMoveX = true;
    if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
        if (isSolid(bx, by, bz)) canMoveX = false;
        if (isSolid(bx, headY, bz)) canMoveX = false;
    }
    if (canMoveX) player.position.x = newX;
    
    // Z movement
    float newZ = player.position.z + moveDir.z;
    bx = (int)floorf(player.position.x);
    bz = (int)floorf(newZ);
    by = (int)floorf(player.position.y);
    headY = (int)floorf(player.position.y + PLAYER_HEIGHT);
    
    bool canMoveZ = true;
    if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
        if (isSolid(bx, by, bz)) canMoveZ = false;
        if (isSolid(bx, headY, bz)) canMoveZ = false;
    }
    if (canMoveZ) player.position.z = newZ;
    
    // Y movement (gravity)
    float newY = player.position.y + player.velocity.y * dt;
    bx = (int)floorf(player.position.x);
    bz = (int)floorf(player.position.z);
    by = (int)floorf(newY);
    headY = (int)floorf(newY + PLAYER_HEIGHT);
    
    player.onGround = false;
    bool canMoveY = true;
    if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
        // Check feet
        if (isSolid(bx, by, bz)) {
            canMoveY = false;
            if (player.velocity.y < 0) {
                player.onGround = true;
                player.velocity.y = 0;
                player.position.y = by + 0.5f;
            }
        }
        // Check head
        if (isSolid(bx, headY, bz)) {
            canMoveY = false;
            if (player.velocity.y > 0) {
                player.velocity.y = 0;
            }
        }
    }
    
    if (canMoveY) {
        player.position.y = newY;
    }
    
    // Keep in bounds
    player.position.x = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, player.position.x));
    player.position.z = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, player.position.z));
}

// ============================================================
//  CREATE GRASS TEXTURE (16x16 pixel art - EXACT Minecraft style)
// ============================================================
Texture2D createFallbackTexture() {
    Image image = GenImageColor(16, 16, WHITE);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *pixels = (Color*)image.data;
    
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int i = y * 16 + x;
            if (y < 6) {
                int r = 80, g = 140, b = 60;
                if ((x + y) % 3 == 0) { r = 100; g = 165; b = 70; }
                if ((x * 2 + y * 3) % 5 == 0) { r = 60; g = 110; b = 45; }
                if (y < 2) { r += 8; g += 10; b += 5; }
                pixels[i] = (Color){r, g, b, 255};
            } else if (y < 8) {
                int r = 100, g = 95, b = 65;
                if ((x + y) % 4 == 0) { r = 85; g = 125; b = 60; }
                pixels[i] = (Color){r, g, b, 255};
            } else {
                int r = 115, g = 90, b = 60;
                if ((x * 3 + y) % 6 == 0) { r += 15; g += 10; b += 5; }
                if ((x + y) % 5 == 0) { r -= 10; g -= 8; b -= 5; }
                pixels[i] = (Color){r, g, b, 255};
            }
        }
    }
    
    Texture2D tex = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    // Init window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "⛏️ Minecraft - Early Days");
    
    // Cursor
    DisableCursor();
    player.cursorLocked = true;
    
    // Set camera mode
    Camera3D camera = {0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Load texture
    grassTexture = LoadTexture("grass.png");
    if (grassTexture.id == 0) {
        TraceLog(LOG_WARNING, "Failed to load grass.png, using fallback texture");
        grassTexture = createFallbackTexture();
    } else {
        SetTextureFilter(grassTexture, TEXTURE_FILTER_POINT);
    }
    
    // Generate world
    generateWorld();
    
    // Set player start position
    int startX = WORLD_SIZE / 2;
    int startZ = WORLD_SIZE / 2;
    player.position.x = startX + 0.5f;
    player.position.z = startZ + 0.5f;
    player.position.y = world[startX][startZ] + PLAYER_HEIGHT;
    
    // Main loop
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Update
        updatePlayer(dt);
        
        // Camera follows player
        camera.position = player.position;
        camera.position.y += 0.1f; // slight bob
        camera.target = (Vector3){
            player.position.x - sinf(player.yaw) * cosf(player.pitch),
            player.position.y + 0.1f + sinf(player.pitch),
            player.position.z - cosf(player.yaw) * cosf(player.pitch)
        };
        
        // Toggle cursor with Escape
        if (IsKeyPressed(KEY_ESCAPE)) {
            player.cursorLocked = !player.cursorLocked;
            if (player.cursorLocked) DisableCursor();
            else EnableCursor();
        }
        
        // Click to lock cursor
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !player.cursorLocked) {
            player.cursorLocked = true;
            DisableCursor();
        }
        
        // Draw
        BeginDrawing();
        ClearBackground((Color){127, 155, 179, 255});
        
        BeginMode3D(camera);
        
        // Draw world
        drawWorld();
        
        EndMode3D();
        
        // Crosshair
        DrawRectangle(screenWidth/2 - 2, screenHeight/2 - 10, 4, 20, (Color){255, 255, 255, 150});
        DrawRectangle(screenWidth/2 - 10, screenHeight/2 - 2, 20, 4, (Color){255, 255, 255, 150});
        
        // HUD
        DrawText("MINECRAFT EARLY DAYS", 10, 10, 20, (Color){255, 255, 255, 150});
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 35, 20, (Color){255, 255, 255, 100});
        
        DrawText("WASD: Walk | SPACE: Jump | Shift: Sprint", 10, screenHeight - 30, 20, (Color){200, 210, 220, 200});
        
        EndDrawing();
    }
    
    // Cleanup
    UnloadTexture(grassTexture);
    CloseWindow();
    
    return 0;
}
