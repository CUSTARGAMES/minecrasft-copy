// ============================================================
//  MINECRAFT BEDROCK STYLE - C++ / Raylib (FIXED API)
//  Compile: g++ -o minecraft.exe minecraft.cpp -lraylib -lopengl32 -lgdi32 -lwinmm
// ============================================================

#include "raylib.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>

// ============================================================
//  CONSTANTS
// ============================================================
const int WORLD_SIZE = 32;
const int BLOCK_SIZE = 1;
const float GRAVITY = -25.0f;
const float JUMP_SPEED = 8.0f;
const float PLAYER_SPEED = 4.5f;
const float PLAYER_HEIGHT = 1.62f;
const float MOUSE_SENSITIVITY = 0.002f;

// ============================================================
//  BLOCK TYPES
// ============================================================
enum BlockType {
    AIR = 0,
    GRASS = 1,
    STONE = 2
};

struct Block {
    BlockType type;
    bool active;
    
    Block() : type(AIR), active(false) {}
    Block(BlockType t) : type(t), active(true) {}
};

// ============================================================
//  WORLD CLASS
// ============================================================
class World {
public:
    Block blocks[WORLD_SIZE][64][WORLD_SIZE];
    Texture2D grassTex;
    Texture2D stoneTex;
    
    World() {
        // Load 16x16 textures
        grassTex = LoadTexture("grass.png");
        stoneTex = LoadTexture("stone.png");
        
        // Fallback textures if files missing
        if (grassTex.id == 0) grassTex = createFallbackTexture(GREEN);
        if (stoneTex.id == 0) stoneTex = createFallbackTexture(GRAY);
        
        // Set texture filter for pixel art (16x16)
        SetTextureFilter(grassTex, TEXTURE_FILTER_POINT);
        SetTextureFilter(stoneTex, TEXTURE_FILTER_POINT);
        
        generateWorld();
    }
    
    ~World() {
        UnloadTexture(grassTex);
        UnloadTexture(stoneTex);
    }
    
    void generateWorld() {
        // Clear world
        for (int x = 0; x < WORLD_SIZE; x++) {
            for (int y = 0; y < 64; y++) {
                for (int z = 0; z < WORLD_SIZE; z++) {
                    blocks[x][y][z] = Block(AIR);
                }
            }
        }
        
        // Flat world: grass on top, stone below
        for (int x = 0; x < WORLD_SIZE; x++) {
            for (int z = 0; z < WORLD_SIZE; z++) {
                // Layer 0-2: Stone
                blocks[x][0][z] = Block(STONE);
                blocks[x][1][z] = Block(STONE);
                blocks[x][2][z] = Block(STONE);
                // Layer 3: Grass
                blocks[x][3][z] = Block(GRASS);
            }
        }
        
        // Add a few stone blocks on surface for decoration
        for (int i = 0; i < 15; i++) {
            int x = rand() % WORLD_SIZE;
            int z = rand() % WORLD_SIZE;
            if (blocks[x][4][z].type == AIR) {
                blocks[x][4][z] = Block(STONE);
            }
        }
    }
    
    BlockType getBlock(int x, int y, int z) {
        if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= 64 || z < 0 || z >= WORLD_SIZE)
            return AIR;
        return blocks[x][y][z].type;
    }
    
    void setBlock(int x, int y, int z, BlockType type) {
        if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= 64 || z < 0 || z >= WORLD_SIZE)
            return;
        blocks[x][y][z] = Block(type);
    }
    
    bool isSolid(int x, int y, int z) {
        return getBlock(x, y, z) != AIR;
    }
    
    Color getBlockColor(BlockType type) {
        switch(type) {
            case GRASS: return (Color){80, 140, 60, 255};
            case STONE: return (Color){128, 128, 128, 255};
            default: return WHITE;
        }
    }
    
    void drawBlock(int x, int y, int z) {
        BlockType type = getBlock(x, y, z);
        if (type == AIR) return;
        
        Vector3 pos = {x + 0.5f, y + 0.5f, z + 0.5f};
        
        // Draw cube with color (texture workaround)
        DrawCubeV(pos, (Vector3){1.0f, 1.0f, 1.0f}, getBlockColor(type));
        
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
                    
                    // Check if block is visible
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
    
private:
    Texture2D createFallbackTexture(Color color) {
        Image img = GenImageColor(16, 16, color);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        return tex;
    }
};

// ============================================================
//  PLAYER CLASS
// ============================================================
class Player {
public:
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
    
    void update(float dt, World& world) {
        // Mouse look
        if (cursorLocked) {
            Vector2 mouseDelta = GetMouseDelta();
            yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
            pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
            pitch = fmaxf(-1.5f, fminf(1.5f, pitch));
        }
        
        // Movement
        Vector3 forward = {-sinf(yaw), 0, -cosf(yaw)};
        Vector3 right = {cosf(yaw), 0, -sinf(yaw)};
        
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
        
        // Flying mode
        if (IsKeyPressed(KEY_F)) {
            flying = !flying;
        }
        
        if (flying) {
            if (IsKeyDown(KEY_SPACE)) moveDir.y += 1.0f;
            if (IsKeyDown(KEY_LEFT_CONTROL)) moveDir.y -= 1.0f;
            
            position.x += moveDir.x * speed * dt;
            position.y += moveDir.y * speed * dt;
            position.z += moveDir.z * speed * dt;
            
            velocity.y = 0;
            onGround = false;
        } else {
            // Jump
            if (IsKeyPressed(KEY_SPACE) && onGround) {
                velocity.y = JUMP_SPEED;
                onGround = false;
            }
            
            // Gravity
            velocity.y += GRAVITY * dt;
            if (velocity.y < -20.0f) velocity.y = -20.0f;
            
            // Collision - X
            float newX = position.x + moveDir.x * speed * dt;
            int bx = (int)floorf(newX);
            int bz = (int)floorf(position.z);
            int by = (int)floorf(position.y);
            int headY = (int)floorf(position.y + PLAYER_HEIGHT);
            
            bool canMoveX = true;
            if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
                if (world.isSolid(bx, by, bz)) canMoveX = false;
                if (world.isSolid(bx, headY, bz)) canMoveX = false;
            }
            if (canMoveX) position.x = newX;
            
            // Collision - Z
            float newZ = position.z + moveDir.z * speed * dt;
            bx = (int)floorf(position.x);
            bz = (int)floorf(newZ);
            by = (int)floorf(position.y);
            headY = (int)floorf(position.y + PLAYER_HEIGHT);
            
            bool canMoveZ = true;
            if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
                if (world.isSolid(bx, by, bz)) canMoveZ = false;
                if (world.isSolid(bx, headY, bz)) canMoveZ = false;
            }
            if (canMoveZ) position.z = newZ;
            
            // Collision - Y
            float newY = position.y + velocity.y * dt;
            bx = (int)floorf(position.x);
            bz = (int)floorf(position.z);
            by = (int)floorf(newY);
            headY = (int)floorf(newY + PLAYER_HEIGHT);
            
            onGround = false;
            bool canMoveY = true;
            if (bx >= 0 && bx < WORLD_SIZE && bz >= 0 && bz < WORLD_SIZE) {
                if (world.isSolid(bx, by, bz)) {
                    canMoveY = false;
                    if (velocity.y < 0) {
                        onGround = true;
                        velocity.y = 0;
                        position.y = by + 0.5f;
                    }
                }
                if (world.isSolid(bx, headY, bz)) {
                    canMoveY = false;
                    if (velocity.y > 0) velocity.y = 0;
                }
            }
            if (canMoveY) position.y = newY;
        }
        
        // Keep in bounds
        position.x = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, position.x));
        position.z = fmaxf(0.5f, fminf(WORLD_SIZE - 0.5f, position.z));
    }
};

// ============================================================
//  MAIN
// ============================================================
int main() {
    // Init window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "⛏️ Minecraft - 16x16 Textures");
    
    // Cursor
    DisableCursor();
    
    // Create world and player
    World world;
    Player player;
    player.cursorLocked = true;
    
    // Camera
    Camera3D camera = {0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Target block
    Vector3 targetBlock = {0, 0, 0};
    bool hasTarget = false;
    BlockType selectedBlock = GRASS;
    
    // Main loop
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Update player
        player.update(dt, world);
        
        // Camera follows player
        camera.position = player.position;
        camera.position.y += 0.1f;
        camera.target = (Vector3){
            player.position.x - sinf(player.yaw) * cosf(player.pitch),
            player.position.y + 0.1f + sinf(player.pitch),
            player.position.z - cosf(player.yaw) * cosf(player.pitch)
        };
        
        // Block selection (1=grass, 2=stone)
        if (IsKeyPressed(KEY_ONE)) selectedBlock = GRASS;
        if (IsKeyPressed(KEY_TWO)) selectedBlock = STONE;
        
        // Raycast for block interaction (manual implementation)
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
                if (world.getBlock(bx, by, bz) != AIR) {
                    targetBlock = {bx + 0.5f, by + 0.5f, bz + 0.5f};
                    hasTarget = true;
                    
                    // Break block
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        world.setBlock(bx, by, bz, AIR);
                    }
                    
                    // Place block
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
                            if (world.getBlock(px, py, pz) == AIR) {
                                world.setBlock(px, py, pz, selectedBlock);
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
        
        // Draw
        BeginDrawing();
        ClearBackground((Color){127, 155, 179, 255});
        
        BeginMode3D(camera);
        
        // Draw world
        world.drawWorld(player.position);
        
        // Draw target highlight
        if (hasTarget) {
            DrawCubeWiresV(targetBlock, (Vector3){1.01f, 1.01f, 1.01f}, BLACK);
        }
        
        EndMode3D();
        
        // Crosshair
        DrawRectangle(screenWidth/2 - 2, screenHeight/2 - 10, 4, 20, (Color){255, 255, 255, 150});
        DrawRectangle(screenWidth/2 - 10, screenHeight/2 - 2, 20, 4, (Color){255, 255, 255, 150});
        
        // HUD
        DrawText("MINECRAFT - 16x16 TEXTURES", 10, 10, 20, (Color){255, 255, 255, 150});
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 35, 20, (Color){255, 255, 255, 100});
        
        // Controls
        DrawText("WASD: Walk | SPACE: Jump | F: Fly", 10, screenHeight - 70, 20, (Color){200, 210, 220, 200});
        DrawText("Left Click: Break | Right Click: Place", 10, screenHeight - 45, 20, (Color){200, 210, 220, 200});
        DrawText("1: Grass | 2: Stone", 10, screenHeight - 20, 20, (Color){200, 210, 220, 200});
        
        // Selected block indicator
        const char* blockNames[] = {"", "GRASS", "STONE"};
        DrawText(TextFormat("Selected: %s", blockNames[selectedBlock]), screenWidth - 200, 10, 20, (Color){255, 255, 255, 200});
        
        // Flying indicator
        if (player.flying) {
            DrawText("✈️ FLYING", screenWidth/2 - 50, 60, 20, (Color){255, 255, 100, 200});
        }
        
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
