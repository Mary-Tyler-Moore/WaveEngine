#pragma once

struct PlayerConfig {
    float speedPixelsPerSecond = 200.0f;
};

struct PlayerInput {
    float moveX = 0.0f;
    float moveY = 0.0f;
};

struct Player {
    PlayerConfig config;
    float x = 0.0f;
    float y = 0.0f;

    void Update(float dtSeconds, const PlayerInput& input) {
        x += input.moveX * config.speedPixelsPerSecond * dtSeconds;
        y += input.moveY * config.speedPixelsPerSecond * dtSeconds;
    }
};
