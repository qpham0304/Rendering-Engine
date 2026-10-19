#pragma once

struct FrameTime {
	double time { 0.0 };            // seconds since the window was created
	float deltaTime { 0.0f };       // real time since the previous frame, clamped, never stopped
	float gameDeltaTime { 0.0f };   // deltaTime while game is playing, 0 while stopped or paused
	float fixedDeltaTime { 0.0f };  // the constant fixed step used by onFixedUpdate
	float alpha { 0.0f };           // how far between the last and the next fixed step for interpolation
};
