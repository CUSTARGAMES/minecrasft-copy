// ============================================================
//  MINECRAFT - FULLY WORKING C++ / Raylib (NO DIRT)
//  Compile: g++ -o minecraft.exe minecraft.cpp -lraylib -lopengl32 -lgdi32 -lwinmm
// ============================================================

#include "raylib.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdio>

// ============================================================
//  CONSTANTS
// ============================================================
const int WORLD_SIZE = 32;
const float GRAVITY = -20.0f;
const float JUMP_SPEED = 7.0f;
const float PLAYER_SPEED = 4.0f;
const float PLAYER_HEIGHT = 1.8f;
const float MOUSE_SENSITIVITY = 0.003f;

// ============================================================
//  BLOCK TYPES
// ============================================================
enum BlockType {
    AIR = 0,
    GRASS = 1,
    STONE = 2
};

// ============================================================
//  TEXTURE MANAGER
// ============================================================
Texture2D grassTex, stoneTex;
bool texturesLoaded = false;

void loadTextures() {
    grassTex = LoadTexture("grass.png");
    stoneTex = LoadTexture("stone.png");
    
    // Check if textures loaded
    if (grassTex.id == 0 || stoneTex.id == 0) {
        texturesLoaded = false;
        printf("ERROR: Failed to load textures! Using fallback colors.\n");
        printf("Make sure grass.png and stone.png are in the same folder.\n");
    } else {
        texturesLoaded = true;
        SetTextureFilter(grassTex, TEXTURE_FILTER_POINT);
        SetTextureFilter(stoneTex, TEXTURE_FILTER_POINT);
        printf("Textures loaded successfully!\n");
    }
}

// ============================================================
//  WORLD DATA
// ============================================================
BlockType world[WORLD_SIZE][64][WORLD_SIZE];

void generateWorld() {
    // Clear world
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < 64; y++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                world[x][y][z] = AIR;
            }
        }
    }
    
    // Flat world: Grass on top, stone underneath (NO DIRT)
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            // Layer 0-2: Stone
            world[x][0][z] = STONE;
            world[x][1][z] = STONE;
            world[x][2][z] = STONE;
            // Layer 3: Grass
            world[x][3][z] = GRASS;
        }
    }
    
    // Add some stone decorations on surface
    for (int i = 0; i < 20; i++) {
        int x = rand() % WORLD_SIZE;
        int z = rand() % WORLD_SIZE;
        if (world[x][4][z] == AIR) {
            world[x][4][z] = STONE;
        }
    }
}

BlockType getBlock(int x, int y, int z) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= 64 || z < 0 || z >= WORLD_SIZE)
        return AIR;
    return world[x][y][z];
}

void setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= 64 || z < 0 || z >= WORLD_SIZE)
        return;
    world[x][y][z] = type;
}

bool isSolid(int x, int y, int z) {
    return getBlock(x, y, z) != AIR;
}

// ============================================================
//  DRAW BLOCK - FIXED (no DrawCubeTexture)
// ============================================================
void drawBlock(int x, int y, int z) {
    BlockType type = getBlock(x, y, z);
    if (type == AIR) return;
    
    Vector3 pos = {x + 0.5f, y + 0.5f, z + 0.5f};
    
    // Colors for fallback
    Color color;
    switch(type) {
        case GRASS: color = (Color){80, 160, 60, 255}; break;
        case STONE: color = (Color){128, 128, 128, 255}; break;
        default: color = WHITE;
    }
    
    // Draw colored cube (works always)
    DrawCubeV(pos, (Vector3){1.0f, 1.0f, 1.0f}, color);
    
    // Very light outline
    DrawCubeWiresV(pos, (Vector3){1.0f, 1.0f, 1.0f}, (Color){0, 0, 0, 30});
}

void drawWorld(Vector3 playerPos) {
    int renderDist = 10;
    int px = (int)floorf(playerPos.x);
    int pz = (int)floorf(playerPos.z);
    
    for (int x = px - renderDist; x < px + renderDist; x++) {
        for (int z = pz - renderDist; z < pz + renderDist; z++) {
            if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE) continue;
            
            for (int y = 0; y < 8; y++) {
                BlockType type = getBlock(x, y, z);
                if (type == AIR) continue;
                
                // Check if block is visible (not fully surrounded)
                bool visible = false;
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

// ============================================================
//  PLAYER
// ============================================================
struct Player {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    bool onGround;
    bool cursorLocked;
    bool flying;
    
    Player() {
        position = {WORLD_SIZE/2.0f, 5.0f, WORLD_SIZE/2.0f};
        velocity = {0, 0, 0};
        yaw = 0;
        pitch = 0;
        onGround = false;
        cursorLocked = false;
        flying = false;
    }
};

void updatePlayer(Player& player, float dt) {
    // Mouse look
    if (player.cursorLocked) {
        Vector2 mouseDelta = GetMouseDelta();
        player.yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
        player.pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
        player.pitch = fmaxf(-1.5f, fminf(1.5f, player.pitch));
    }
    
    // Movement direction
    Vector3 forward = {-sinf(player.yaw), 0, -cosf(player.yaw)};
    Vector3 right = {cosf(player.yaw), 0, -sinf(player.yaw)};
    
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
    
    float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 6.0f : PLAYER_SPEED;
    
    // Toggle flying
    if (IsKeyPressed(KEY_F)) {
        player.flying = !player.flying;
    }
    
    if (player.flying) {
        // Flying mode
        if (IsKeyDown(KEY_SPACE)) moveDir.y += 1.0f;
        if (IsKeyDown(KEY_LEFT_CONTROL)) moveDir.y -= 1.0f;
        
        player.position.x += moveDir.x * speed * dt;
        player.position.y += moveDir.y * speed * dt;
        player.position.z += moveDir.z * speed * dt;
        
        player.velocity.y = 0;
        player.onGround = false;
    } else {
        // Normal physics
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
        float newX = player.position.x + moveDir.x * speed * dt;
        int bx = (int)floorf(newX + 0.3f);
        int bz = (int)floorf(player.position.z + 0.3f);
        int by = (int)floorf(player.position.y);
        int headY = (int)floorf(player.position.y + PLAYER_HEIGHT);
        
        bool canMoveX = true;
        if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
            if (isSolid(bx, by, bz)) canMoveX = false;
            if (isSolid(bx, headY, bz)) canMoveX = false;
        }
        if (canMoveX) player.position.x = newX;
        
        // Z movement
        float newZ = player.position.z + moveDir.z * speed * dt;
        bx = (int)floorf(player.position.x + 0.3f);
        bz = (int)floorf(newZ + 0.3f);
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
        bx = (int)floorf(player.position.x + 0.3f);
        bz = (int)floorf(player.position.z + 0.3f);
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
        if (canMoveY) player.position.y = newY;
    }
    
    // Keep in bounds
    player.position.x = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, player.position.x));
    player.position.z = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, player.position.z));
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    srand(time(NULL));
    
    // Init window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "⛏️ Minecraft - C++ Edition");
    
    // Load textures
    loadTextures();
    
    // Generate world
    generateWorld();
    
    // Create player
    Player player;
    player.cursorLocked = true;
    DisableCursor();
    
    // Camera
    Camera3D camera = {0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Block interaction
    BlockType selectedBlock = GRASS;
    Vector3 targetBlock = {0, 0, 0};
    bool hasTarget = false;
    
    // Main loop
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Update player
        updatePlayer(player, dt);
        
        // Camera follows player
        camera.position = player.position;
        camera.position.y += 0.1f;
        camera.target = (Vector3){
            player.position.x - sinf(player.yaw) * cosf(player.pitch),
            player.position.y + 0.1f + sinf(player.pitch),
            player.position.z - cosf(player.yaw) * cosf(player.pitch)
        };
        
        // Block selection
        if (IsKeyPressed(KEY_ONE)) selectedBlock = GRASS;
        if (IsKeyPressed(KEY_TWO)) selectedBlock = STONE;
        
        // Raycast for block interaction
        hasTarget = false;
        Vector3 rayStart = camera.position;
        Vector3 rayDir = {
            camera.target.x - camera.position.x,
            camera.target.y - camera.position.y,
            camera.target.z - camera.position.z
        };
        float len = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
        rayDir.x /= len;
        rayDir.y /= len;
        rayDir.z /= len;
        
        for (float d = 0.1f; d < 6.0f; d += 0.05f) {
            Vector3 check = {
                rayStart.x + rayDir.x * d,
                rayStart.y + rayDir.y * d,
                rayStart.z + rayDir.z * d
            };
            int bx = (int)floorf(check.x);
            int by = (int)floorf(check.y);
            int bz = (int)floorf(check.z);
            
            if (bx >= 0 && bx < WORLD_SIZE && by >= 0 && by < 64 && bz >= 0 && bz < WORLD_SIZE) {
                if (getBlock(bx, by, bz) != AIR) {
                    targetBlock = {bx + 0.5f, by + 0.5f, bz + 0.5f};
                    hasTarget = true;
                    
                    // Break block (left click)
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        setBlock(bx, by, bz, AIR);
                    }
                    
                    // Place block (right click)
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        Vector3 placePos = {
                            check.x + rayDir.x * 0.1f,
                            check.y + rayDir.y * 0.1f,
                            check.z + rayDir.z * 0.1f
                        };
                        int px = (int)floorf(placePos.x);
                        int py = (int)floorf(placePos.y);
                        int pz = (int)floorf(placePos.z);
                        
                        if (px >= 0 && px < WORLD_SIZE && py >= 0 && py < 64 && pz >= 0 && pz < WORLD_SIZE) {
                            if (getBlock(px, py, pz) == AIR) {
                                setBlock(px, py, pz, selectedBlock);
                            }
                        }
                    }
                    break;
                }
            }
        }
        
        // Toggle cursor
        if (IsKeyPressed(KEY_ESCAPE)) {
            player.cursorLocked = !player.cursorLocked;
            if (player.cursorLocked) DisableCursor();
            else EnableCursor();
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !player.cursorLocked) {
            player.cursorLocked = true;
            DisableCursor();
        }
        
        // ============================================================
        //  RENDER
        // ============================================================
        BeginDrawing();
        ClearBackground((Color){127, 155, 179, 255});
        
        BeginMode3D(camera);
        
        // Draw world
        drawWorld(player.position);
        
        // Draw target highlight
        if (hasTarget) {
            DrawCubeWiresV(targetBlock, (Vector3){1.01f, 1.01f, 1.01f}, BLACK);
        }
        
        EndMode3D();
        
        // Crosshair
        DrawRectangle(screenWidth/2 - 2, screenHeight/2 - 10, 4, 20, (Color){255, 255, 255, 150});
        DrawRectangle(screenWidth/2 - 10, screenHeight/2 - 2, 20, 4, (Color){255, 255, 255, 150});
        
        // HUD
        DrawText("MINECRAFT - C++", 10, 10, 20, (Color){255, 255, 255, 150});
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 35, 20, (Color){255, 255, 255, 100});
        
        // Controls info
        DrawText("WASD: Walk | SPACE: Jump | F: Fly | Shift: Sprint", 10, screenHeight - 80, 20, (Color){200, 210, 220, 200});
        DrawText("Left Click: Break | Right Click: Place", 10, screenHeight - 55, 20, (Color){200, 210, 220, 200});
        DrawText("1: Grass | 2: Stone", 10, screenHeight - 30, 20, (Color){200, 210, 220, 200});
        
        // Selected block
        const char* blockNames[] = {"AIR", "GRASS", "STONE"};
        DrawText(TextFormat("Selected: %s", blockNames[selectedBlock]), screenWidth - 200, 10, 20, (Color){255, 255, 255, 200});
        
        // Flying indicator
        if (player.flying) {
            DrawText("✈️ FLYING", screenWidth/2 - 50, 60, 20, (Color){255, 255, 100, 200});
        }
        
        // Texture status
        if (!texturesLoaded) {
            DrawText("⚠️ Textures not found! Using colors.", screenWidth/2 - 150, screenHeight - 150, 20, (Color){255, 200, 100, 200});
        }
        
        EndDrawing();
    }
    
    // Cleanup
    if (texturesLoaded) {
        UnloadTexture(grassTex);
        UnloadTexture(stoneTex);
    }
    CloseWindow();
    
    return 0;
}
